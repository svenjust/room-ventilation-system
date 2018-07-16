/*
 * Copyright (C) 2018 Sven Just (sven@familie-just.de)
 * Copyright (C) 2018 Ivan Schréter (schreter@gmx.net)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This copyright notice MUST APPEAR in all copies of the software!
 */

#include "Antifreeze.h"
#include "KWLConfig.h"
#include "KWLPersistentConfig.h"
#include "StringView.h"
#include "TempSensors.h"
#include "FanControl.h"
#include "MQTTTopic.hpp"

#include <Wire.h>

/// Run the check every minute.
static constexpr unsigned long INTERVAL_ANTIFREEZE_CHECK = 60000000;

/// Check if preheater increased the temperature after 10 minutes.
static constexpr unsigned long INTERVAL_ANTIFREEZE_ALARM_CHECK = 600000;
// 600000 = 10 * 60 * 1000;   // 10 Min Zeitraum zur Überprüfung, ob Vorheizregister die Temperatur erhöhen kann

/// Time to turn off fans when antifreeze is used in combination with heating appliance (4h).
static constexpr unsigned long INTERVAL_HEATING_APP_COMB_USE_ANTIFREEZE = 14400000;   // 4 Stunden = 4 *60 * 60 * 1000

/// Threshold exhaust air temperature under which to do antifreeze processing.
static constexpr double EXHAUST_ANTIFREEZE_TEMP_THRESHOLD = 1.5;     // Nach kaltem Wetter im Feb 2018 gemäß Messwerte

/// Maximum hysteresis temperature, which makes sense.
static constexpr long MAX_TEMP_HYSTERESIS = 10;

// PID REGLER
static constexpr double heaterKp = 50, heaterKi = 0.1, heaterKd = 0.025;

Antifreeze::Antifreeze(FanControl& fan, TempSensors& temp, KWLPersistentConfig& config) :
  fan_(fan),
  temp_(temp),
  config_(config),
  hysteresis_temp_delta_(KWLConfig::StandardAntifreezeHystereseTemp),
  pid_preheater_(&temp_.get_t4_exhaust(), &tech_setpoint_preheater_, &antifreeze_temp_upper_limit_, heaterKp, heaterKi, heaterKd, P_ON_M, DIRECT),
  heating_app_comb_use_(KWLConfig::StandardHeatingAppCombUse != 0),
  stats_(F("Antifreeze")),
  timer_task_(stats_, &Antifreeze::run, *this)
{}

void Antifreeze::begin(Print& /*initTracer*/)
{
  // antifreeze
  hysteresis_temp_delta_ = config_.getAntifreezeHystereseTemp(); // TODO variable name is wrong
  antifreeze_temp_upper_limit_ = EXHAUST_ANTIFREEZE_TEMP_THRESHOLD + hysteresis_temp_delta_;

  pid_preheater_.SetOutputLimits(100, 1000);
  pid_preheater_.SetMode(MANUAL);
  pid_preheater_.SetSampleTime(1000 /* TODO use constant intervalSetFan */);  // SetFan ruft Preheater auf, deswegen hier intervalSetFan

  heating_app_comb_use_ = (config_.getHeatingAppCombUse() != 0);

  timer_task_.runRepeated(INTERVAL_ANTIFREEZE_CHECK);
  sendMQTT();
}

void Antifreeze::setHeatingAppCombUse(bool on)
{
  if (heating_app_comb_use_ != on) {
    heating_app_comb_use_ = on;
    if (KWLConfig::serialDebug) {
      if (on)
        Serial.println(F("Feuerstättenmodus wird aktiviert und gespeichert"));
      else
        Serial.println(F("Feuerstättenmodus wird DEAKTIVIERT und gespeichert"));
    }
    config_.setHeatingAppCombUse(on);
  }
  forceSend();
}

void Antifreeze::forceSend()
{
  sendMQTT();
}

void Antifreeze::run()
{
  // Funktion wird regelmäßig zum Überprüfen ausgeführt
  if (KWLConfig::serialDebugAntifreeze)
    Serial.println(F("Antifreeze: check start"));

  // antifreeze_state_ = aktueller Status der AntiFrostSchaltung
  // Es wird in jeden Status überprüft, ob die Bedingungen für einen Statuswechsel erfüllt sind
  // Wenn ja, werden die einmaligen Aktionen (Setzen von Variablen) hier ausgeführt.
  // siehe auch "/Docs/Programming/Zustandsänderung Antifreeze.jpg"
  // Die regelmäßigen Aktionen für jedem Status werden in DoActionAntiFreezeState ausgeführt.
  bool send_mqtt = false;
  switch (antifreeze_state_) {

    case AntifreezeState::OFF:
      if ((temp_.get_t4_exhaust() <= EXHAUST_ANTIFREEZE_TEMP_THRESHOLD)
          && (temp_.get_t1_outside() < 0.0)
          && (temp_.get_t4_exhaust() > TempSensors::INVALID)
          && (temp_.get_t1_outside() > TempSensors::INVALID))
        // Wenn Sensoren fehlen, ist der Wert INVALID
      {
        // Neuer Status: AntifreezeState::PREHEATER
        antifreeze_state_ = AntifreezeState::PREHEATER;
        send_mqtt = true;

        // Vorheizer einschalten
        antifreeze_temp_upper_limit_  = EXHAUST_ANTIFREEZE_TEMP_THRESHOLD + hysteresis_temp_delta_;
        pid_preheater_.SetMode(AUTOMATIC);  // Pid einschalten
        preheater_start_time_ms_ = millis();

        if (KWLConfig::serialDebugAntifreeze)
          Serial.println(F("Antifreeze: threshold reached; state = PREHEATER"));
      }
      break;

    case AntifreezeState::PREHEATER:
      // Vorheizregister ist an
      if (temp_.get_t4_exhaust() > EXHAUST_ANTIFREEZE_TEMP_THRESHOLD + hysteresis_temp_delta_) {
        // Neuer Status: AntifreezeState::OFF
        antifreeze_state_ = AntifreezeState::OFF;
        send_mqtt = true;
        pid_preheater_.SetMode(MANUAL);
        if (KWLConfig::serialDebugAntifreeze)
          Serial.println(F("Antifreeze: threshold reached; state = OFF"));
      } else if ((millis() - preheater_start_time_ms_ > INTERVAL_ANTIFREEZE_ALARM_CHECK)
          && (temp_.get_t4_exhaust() <= EXHAUST_ANTIFREEZE_TEMP_THRESHOLD)
          && (temp_.get_t1_outside() < 0.0)
          && (temp_.get_t4_exhaust() > TempSensors::INVALID)
          && (temp_.get_t1_outside() > TempSensors::INVALID)) {
        // 10 Minuten vergangen seit antifreeze_state_ == AntifreezeState::PREHEATER und Temperatur immer noch unter EXHAUST_ANTIFREEZE_TEMP_THRESHOLD
        if (heating_app_comb_use_) {
          // Neuer Status: AntifreezeState::FIREPLACE
          antifreeze_state_ =  AntifreezeState::FIREPLACE;
          send_mqtt = true;
          pid_preheater_.SetMode(MANUAL);
          // Zeit speichern
          heating_app_comb_use_antifreeze_start_time_ms_ = millis();
          if (KWLConfig::serialDebugAntifreeze)
            Serial.println(F("Antifreeze: preheater timeout; state = FIREPLACE"));
        } else {
          // Neuer Status: AntifreezeState::FAN_OFF
          antifreeze_state_ = AntifreezeState::FAN_OFF;
          send_mqtt = true;
          pid_preheater_.SetMode(MANUAL);
          if (KWLConfig::serialDebugAntifreeze)
            Serial.println(F("Antifreeze: preheater timeout; state = FAN_OFF"));
        }
        break;

      // Zuluftventilator ist aus, kein KAMIN angeschlossen
      case AntifreezeState::FAN_OFF:  // antifreeze_state_ = 2
        if (temp_.get_t4_exhaust() > EXHAUST_ANTIFREEZE_TEMP_THRESHOLD + hysteresis_temp_delta_) {
          // Neuer Status: AntifreezeState::OFF
          antifreeze_state_ = AntifreezeState::OFF;
          send_mqtt = true;
          pid_preheater_.SetMode(MANUAL);
        }
        break;

      // Zu- und Abluftventilator sind für vier Stunden aus, KAMINMODUS
      case AntifreezeState::FIREPLACE:  // antifreeze_state_ = 4
        if (millis() - heating_app_comb_use_antifreeze_start_time_ms_ > INTERVAL_HEATING_APP_COMB_USE_ANTIFREEZE) {
          // Neuer Status: AntifreezeState::OFF
          antifreeze_state_ = AntifreezeState::OFF;
          send_mqtt = true;
          pid_preheater_.SetMode(MANUAL);
        }
        break;
      }
  }

  if (KWLConfig::serialDebugAntifreeze) {
    Serial.print (F("millis: "));
    Serial.println (millis());
    Serial.print (F("antifreeze_state_: "));
    Serial.println(uint8_t(antifreeze_state_));
  }

  if (send_mqtt)
    sendMQTT();
}

void Antifreeze::setPreheater()
{
  // Das Vorheizregister wird durch ein PID geregelt
  // Die Stellgröße darf zwischen 0..10V liegen
  // Wenn der Zuluftventilator unter einer Schwelle des Tachosignals liegt, wird das Vorheizregister IMMER ausgeschaltet (SICHERHEIT)
  // Schwelle: 1000 U/min

  if (KWLConfig::serialDebugAntifreeze) {
    Serial.println(F("SetPreheater start"));
  }

  // Sicherheitsabfrage
  if (fan_.getFan1().getSpeed() < 600 || fan_.getFan1().isOff()) {
    // Sicherheitsabschaltung Vorheizer unter 600 Umdrehungen Zuluftventilator
    tech_setpoint_preheater_ = 0;
  }
  if (KWLConfig::serialDebugAntifreeze) {
    Serial.print(F("Preheater - M: "));
    Serial.print(millis());
    Serial.print(F(", Gap: "));
    Serial.print(abs(antifreeze_temp_upper_limit_ - temp_.get_t4_exhaust()));
    Serial.print(F(", tech_setpoint_preheater_: "));
    Serial.println(tech_setpoint_preheater_);
    // TODO based on what to send via MQTT?
    //MessageHandler::publish(MQTTTopic::KwlDebugstatePreheater, _buffer);
  }

  // Setzen per PWM
  unsigned tech_setpoint = unsigned(tech_setpoint_preheater_);
  analogWrite(KWLConfig::PinPreheaterPWM, tech_setpoint / 4);

  // Setzen der Werte per DAC
  byte HBy = byte(tech_setpoint >> 8);
  byte LBy = tech_setpoint & 0xff;

  Wire.beginTransmission(KWLConfig::DacI2COutAddr); // Start Übertragung zur ANALOG-OUT Karte
  Wire.write(KWLConfig::DacChannelPreheater);        // PREHEATER schreiben
  Wire.write(LBy);                          // LOW-Byte schreiben
  Wire.write(HBy);                          // HIGH-Byte schreiben
  Wire.endTransmission();                   // Ende
}

void Antifreeze::sendMQTT()
{
  uint8_t bitmask = 3;
  mqtt_publish_.publish([this, bitmask]() mutable {
    if (!publish_if(bitmask, uint8_t(1), MQTTTopic::KwlAntifreeze,
                    (antifreeze_state_ != AntifreezeState::OFF) ? F("on") : F("off"),
                    KWLConfig::RetainAntifreezeState))
      return false;
    if (!publish_if(bitmask, uint8_t(2), MQTTTopic::KwlHeatingAppCombUse,
                    heating_app_comb_use_ ? F("YES") : F("NO"),
                    KWLConfig::RetainAntifreezeState))
      return false;
    return true;
  });
}

void Antifreeze::doActionAntiFreezeState()
{
  // Funktion wird ausgeführt, um AntiFreeze (Frostschutz) zu erreichen.
  switch (antifreeze_state_)
  {
    case AntifreezeState::PREHEATER:
      pid_preheater_.Compute();
      break;

    case AntifreezeState::FAN_OFF:
      // Zuluft aus
      if (KWLConfig::serialDebugAntifreeze)
        Serial.println(F("Antifreeze: fan1 = 0"));
      fan_.getFan1().off();
      tech_setpoint_preheater_ = 0;
      break;

    case AntifreezeState::FIREPLACE:
      // Feuerstättenmodus
      // beide Lüfter aus
      if (KWLConfig::serialDebugAntifreeze)
        Serial.println(F("Antifreeze: fan1 = 0, fan2 = 0 (fireplace)"));
      fan_.getFan1().off();
      fan_.getFan2().off();
      tech_setpoint_preheater_ = 0;
      break;

    default:
      // Normal Mode without AntiFreeze
      // Vorheizregister aus
      tech_setpoint_preheater_ = 0;
      break;
  }

  setPreheater();
}

bool Antifreeze::mqttReceiveMsg(const StringView& topic, const StringView& s)
{
  if (KWLConfig::serialDebug)
    Serial.println(F("MQTT handler: Antifreeze"));

  if (topic == MQTTTopic::CmdAntiFreezeHyst) {
    auto i = s.toInt();
    if (i < 0)
      i = 0;
    if (i > MAX_TEMP_HYSTERESIS)
      i = MAX_TEMP_HYSTERESIS;
    hysteresis_temp_delta_ = unsigned(i);
    antifreeze_temp_upper_limit_ = EXHAUST_ANTIFREEZE_TEMP_THRESHOLD + hysteresis_temp_delta_;
    config_.setAntifreezeHystereseTemp(hysteresis_temp_delta_);
  } else if (topic == MQTTTopic::CmdHeatingAppCombUse) {
    if (s == F("YES"))
      setHeatingAppCombUse(true);
    else if (s == F("NO"))
      setHeatingAppCombUse(false);
  } else {
    return false;
  }
  return true;
}
