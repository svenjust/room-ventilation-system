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

#include "FanControl.h"
#include "MQTTTopic.hpp"
#include "KWLConfig.h"

#include <StringView.h>

#include <Arduino.h>
#include <Wire.h>

/// Global instance used by interrupt routines.
static FanControl* instance_ = nullptr;

// MQTT timing:

/// Interval for scheduling fan regulation (1s)
static constexpr unsigned long FAN_INTERVAL = 1000000;
/// Interval for sending fan information (5s), if speed changed.
static constexpr unsigned long FAN_MQTT_INTERVAL = 5000000;
/// Interval for sending fan information unconditionally (2min).
static constexpr unsigned long FAN_MQTT_INTERVAL_OVERSAMPLING = 120000000;
/// Interval for sending mode information unconditionally (5min).
static constexpr unsigned long MODE_MQTT_INTERVAL = 300000000;
/// Only send fan speed if changed by at least 50rpm.
static constexpr int MIN_SPEED_DIFF = 50;

// Calibration timing:

/// Timeout for the entire calibration (10 minutes). If the calibration doesn't
/// succeed in time, it will be cancelled.
static constexpr unsigned long TIMEOUT_CALIBRATION = 600000000;
/// Timeout for the calibration of one PWM step (5 minutes).
static constexpr unsigned long TIMEOUT_PWM_CALIBRATION = 300000000;

// Define the aggressive and conservative Tuning Parameters
// Nenndrehzahl Lüfter 3200, Stellwert 0..1000 entspricht 0-10V
static constexpr double aggKp  = 0.5,  aggKi = 0.1, aggKd  = 0.001;
static constexpr double consKp = 0.1, consKi = 0.1, consKd = 0.001;

Fan::Fan(uint8_t id, uint8_t powerPin, uint8_t pwmPin, uint8_t tachoPin, float multiplier) :
  rpm_(static_cast<FanRPM::multiplier_t>(multiplier * FanRPM::RPM_MULTIPLIER_BASE)),
  power_(powerPin),
  pwm_pin_(pwmPin),
  tacho_pin_(tachoPin),
  fan_id_(id),
  pid_(&current_speed_, &tech_setpoint_, &speed_setpoint_, consKp, consKi, consKd, P_ON_M, DIRECT)
{}

void Fan::begin(void (*countUp)(), unsigned standardSpeed, float multiplier)
{
  standard_speed_ = standardSpeed;
  rpm_.multiplier() = static_cast<FanRPM::multiplier_t>(multiplier * FanRPM::RPM_MULTIPLIER_BASE);

  pid_.SetOutputLimits(0, 1000);
  pid_.SetMode(AUTOMATIC);
  pid_.SetSampleTime(1000); // TODO use constant slightly less than in fan interval?

  // Lüfter Speed
  pinMode(pwm_pin_, OUTPUT);
  digitalWrite(pwm_pin_, LOW);

  // Lüfter Tacho Interrupt
  pinMode(tacho_pin_, INPUT_PULLUP);
  auto intr = uint8_t(digitalPinToInterrupt(tacho_pin_));
  attachInterrupt(intr, countUp, KWLConfig::TachoSamplingMode);

  Serial.print(F("Fan pins(tacho/PWM), interrupt, std speed, mult:\t"));
  Serial.print(tacho_pin_);
  Serial.print('\t');
  Serial.print(pwm_pin_);
  Serial.print('\t');
  Serial.print(intr);
  Serial.print('\t');
  Serial.print(standardSpeed);
  Serial.print('\t');
  Serial.println(multiplier);

  // Turn on power
  power_.on();
}

void Fan::computeSpeed(int ventMode, FanCalculateSpeedMode calcMode)
{
  speed_setpoint_ = standard_speed_ * KWLConfig::StandardKwlModeFactor[ventMode];

  if (ventMode == 0) {
    tech_setpoint_ = 0 ;  // Lüfungsstufe 0 alles ausschalten
    return;
  }

  double gap = abs(speed_setpoint_ - current_speed_); //distance away from setpoint

  // Das PWM-Signal kann entweder per PID-Regler oder unten per Dreisatz berechnen werden.
  // TODO above comment seems invalid now
  if (calcMode == FanCalculateSpeedMode::PID) {
    if (gap < 1000) {
      pid_.SetTunings(consKp, consKi, consKd);
    } else {
      pid_.SetTunings(aggKp, aggKi, aggKd);
    }
    pid_.Compute();
  } else if (calcMode == FanCalculateSpeedMode::PROP) {
    tech_setpoint_ = pwm_setpoint_[ventMode];
  }

  // Grenzwertbehandlung: Max- / Min-Werte
  if (tech_setpoint_ < 0)
    tech_setpoint_ = 0;
  if (tech_setpoint_ > 1000)
    tech_setpoint_ = 1000;
}

void Fan::setSpeed(int id, uint8_t pwmPin, uint8_t dacChannel)
{
  if (KWLConfig::serialDebugFan) {
    Serial.print(F("Fan "));
    Serial.print(id);
    Serial.print(F(": \tgap: "));
    Serial.print(current_speed_ - speed_setpoint_);
    Serial.print(F("\tspeedTacho: "));
    Serial.print(current_speed_);
    Serial.print(F("\ttechSetpoint: "));
    Serial.print(tech_setpoint_);
    Serial.print(F("\tspeedSetpoint: "));
    Serial.println(speed_setpoint_);
    rpm_.dump(Serial);
  }

  // Setzen per PWM
  // Ausgabewert für Lüftersteuerung darf zwischen 0-10V liegen, dies entspricht 0..1023, vereinfacht 0..1000
  // 0..1000 muss umgerechnet werden auf 0..255 also durch 4 geteilt werden
  // max. Lüfterdrehzahl bei Papstlüfter 3200 U/min
  int tech = int(tech_setpoint_);
  analogWrite(pwmPin, tech / 4);

  // Setzen der Werte per DAC
  if (KWLConfig::ControlFansDAC) {
    byte HBy;
    byte LBy;

    HBy = byte(tech >> 8);   //HIGH-Byte berechnen
    LBy = byte(tech & 255);  //LOW-Byte berechnen
    Wire.beginTransmission(KWLConfig::DacI2COutAddr); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(dacChannel);                   // Fan channel schreiben
    Wire.write(LBy);                          // LOW-Byte schreiben
    Wire.write(HBy);                          // HIGH-Byte schreiben
    Wire.endTransmission();                   // Ende
  }
}

void Fan::debugSet(int ventMode, int techSetpoint) {
  if (techSetpoint < 0)
    techSetpoint = 0;
  else if (techSetpoint > 1000)
    techSetpoint = 1000;
  pwm_setpoint_[ventMode] = techSetpoint;
}

bool Fan::speedCalibrationStep(int mode)
{
  if (abs(KWLConfig::StandardKwlModeFactor[mode]) < 0.01) {
    // Faktor Null ist einfach
    calibration_pwm_setpoint_[mode] = 0;
    return true;
  } else {
    // Faktor ungleich 0
    speed_setpoint_ = standard_speed_ * KWLConfig::StandardKwlModeFactor[mode];

    int maxGap = (speed_setpoint_ / 100 * KWLConfig::StandardKwlFanPrecisionPercent) + 1 ;  // max. StandardKwlFanPrecisionPercent % Abweichung 
    double gap = abs(speed_setpoint_ - current_speed_); //distance away from setpoint
    if ((gap < maxGap) && (good_pwm_setpoint_count_ < REQUIRED_GOOD_PWM_COUNT)) {
      // einen PWM Wert gefunden
      good_pwm_setpoint_[good_pwm_setpoint_count_] = int(tech_setpoint_);
      good_pwm_setpoint_count_++;
    }
    if (good_pwm_setpoint_count_ >= REQUIRED_GOOD_PWM_COUNT) {
      // fertig, genug Werte gefunden, jetzt Durchschnitt bilden
      long sum_pwm = 0;
      for (unsigned i = 0; i < REQUIRED_GOOD_PWM_COUNT; i++) {
        sum_pwm += good_pwm_setpoint_[i];
      }
      calibration_pwm_setpoint_[mode] = int(sum_pwm / REQUIRED_GOOD_PWM_COUNT);
      return true;
    }

    // Noch nicht genug Werte, PID Regler muss nachregeln
    if (gap < 1000)
      pid_.SetTunings(consKp, consKi, consKd);
    else
      pid_.SetTunings(aggKp, aggKi, aggKd);
    pid_.Compute();

    // !Kein PreHeating und keine Sicherheitsabfrage Temperatur
    return false;
  }
}

void Fan::finishCalibration()
{
  for (unsigned i = 0; (i < KWLConfig::StandardModeCnt) && (i < MAX_FAN_MODE_CNT); ++i)
    pwm_setpoint_[i] = calibration_pwm_setpoint_[i];
}

void Fan::sendMQTTDebug(int id, unsigned long ts, MessageHandler& h)
{
  if (!mqtt_send_debug_)
    return;

  char buffer[100];
  static constexpr auto FORMAT = makeFlashStringLiteral("Fan%d - M: %lu, gap: %ld, tsf: %ld, ssf: %ld, rpm: %ld");
  snprintf(buffer, sizeof(buffer), FORMAT.load(),
           id, ts,
           long(current_speed_ - speed_setpoint_),
           long(tech_setpoint_), long(speed_setpoint_), long(current_speed_));
  h.publish((id == 1) ? MQTTTopic::KwlDebugstateFan1 : MQTTTopic::KwlDebugstateFan2, buffer, false);
}


FanControl::FanControl(KWLPersistentConfig& config, SetSpeedCallback *speedCallback) :
  MessageHandler(F("FanControl")),
  fan1_(1, KWLConfig::PinFan1Power, KWLConfig::PinFan1PWM, KWLConfig::PinFan1Tacho, KWLConfig::StandardFan1Multiplier),
  fan2_(2, KWLConfig::PinFan2Power, KWLConfig::PinFan2PWM, KWLConfig::PinFan2Tacho, KWLConfig::StandardFan2Multiplier),
  speed_callback_(speedCallback),
  ventilation_mode_(KWLConfig::StandardKwlMode),
  persistent_config_(config),
  stats_(F("FanControl")),
  timer_task_(stats_, &FanControl::run, *this)
{}

void FanControl::begin(Print& initTrace)
{
  initTrace.println(F("Initialisierung Ventilatoren"));

  instance_ = this;
  for (unsigned i = 0; ((i < KWLConfig::StandardModeCnt) && (i < 10)); i++) {
    fan1_.initPWM(i, persistent_config_.getFanPWMSetpoint(0, i));
    fan2_.initPWM(i, persistent_config_.getFanPWMSetpoint(1, i));
  }
  fan1_.begin(countUpFan1, persistent_config_.getSpeedSetpointFan1(), persistent_config_.getFan1Multiplier());
  fan2_.begin(countUpFan2, persistent_config_.getSpeedSetpointFan2(), persistent_config_.getFan2Multiplier());

  timer_task_.runRepeated(FAN_INTERVAL);
}

void FanControl::setVentilationMode(int mode)
{
  ventilation_mode_ = constrain(mode, 0, int(KWLConfig::StandardModeCnt - 1));
  speedUpdate();
  forceSendMode();
}

void FanControl::countUpFan1() { instance_->fan1_.interrupt(); }

void FanControl::countUpFan2() { instance_->fan2_.interrupt(); }

void FanControl::run()
{
  // Die Geschwindigkeit der beiden Lüfter wird bestimmt. Die eigentliche Zählung der Tachoimpulse
  // geschieht per Interrupt in countUpFan1 und countUpFan2

  fan1_.updateSpeed();
  fan2_.updateSpeed();

  if (KWLConfig::serialDebugFan) {
    Serial.print(F("Speed fan1: "));
    Serial.print(fan1_.getSpeed());
    Serial.print(F(", fan2: "));
    Serial.print(fan2_.getSpeed());
    if (mode_ == FanMode::Calibration)
      Serial.print(F(" [calibration]"));
    Serial.println();
  }

  if (mode_ == FanMode::Normal) {
    speedUpdate();
  } else if (mode_ == FanMode::Calibration) {
    speedCalibrationStep();
  }

  // publish any measurements, if necessary
  bool send_mqtt = false;
  if (--send_mode_countdown_ <= 0) {
    send_mode_countdown_ = int(MODE_MQTT_INTERVAL / FAN_INTERVAL);
    mqtt_send_flags_ |= MQTT_SEND_MODE;
    send_mqtt = true;
  }
  if (--send_fan_oversampling_countdown_ <= 0) {
    send_fan_oversampling_countdown_ = int(FAN_MQTT_INTERVAL_OVERSAMPLING / FAN_INTERVAL);
    send_fan_countdown_ = int(FAN_MQTT_INTERVAL / FAN_INTERVAL);
    mqtt_send_flags_ |= MQTT_SEND_FAN1 | MQTT_SEND_FAN2;
    send_mqtt = true;
  }
  if (--send_fan_countdown_ <= 0) {
    int fan1 = int(fan1_.getSpeed());
    int fan2 = int(fan2_.getSpeed());
    // check whether we need to send data
    if (abs(fan1 - last_sent_fan1_speed_) >= MIN_SPEED_DIFF ||
        abs(fan2 - last_sent_fan2_speed_) >= MIN_SPEED_DIFF) {
      send_fan_oversampling_countdown_ = int(FAN_MQTT_INTERVAL_OVERSAMPLING / FAN_INTERVAL);
      send_fan_countdown_ = int(FAN_MQTT_INTERVAL / FAN_INTERVAL);
      mqtt_send_flags_ |= MQTT_SEND_FAN1 | MQTT_SEND_FAN2;
      send_mqtt = true;
    }
  }

  if (send_mqtt)
    sendMQTT();
}

void FanControl::speedUpdate()
{
  fan1_.computeSpeed(ventilation_mode_, calc_speed_mode_);
  fan2_.computeSpeed(ventilation_mode_, calc_speed_mode_);

  if (speed_callback_)
    speed_callback_->fanSpeedSet();

  setSpeed();
}

void FanControl::setSpeed()
{
  fan1_.sendMQTTDebug(1, timer_task_.getScheduleTime(), *this);
  fan2_.sendMQTTDebug(2, timer_task_.getScheduleTime(), *this);

  if (KWLConfig::serialDebugFan) {
    Serial.print(F("Timestamp: "));
    Serial.println(timer_task_.getScheduleTime());
  }
  fan1_.setSpeed(1, KWLConfig::PinFan1PWM, KWLConfig::DacChannelFan1);
  fan2_.setSpeed(2, KWLConfig::PinFan2PWM, KWLConfig::DacChannelFan2);
}

void FanControl::speedCalibrationStart() {
  Serial.println(F("Kalibrierung der Lüfter wird gestartet"));
  calibration_pwm_in_progress_ = false;
  calibration_in_progress_ = false;
  mode_ = FanMode::Calibration;
}

void FanControl::speedCalibrationStep()
{
  Serial.println(F("SpeedCalibrationPwm startet"));
  if (!calibration_in_progress_) {
    // Erster Durchlauf der Kalibrierung
    Serial.println(F("Erster Durchlauf"));
    calibration_in_progress_ = true;
    calibration_start_time_us_ = timer_task_.getScheduleTime();
    current_calibration_mode_ = 0;
    calc_speed_mode_ = FanCalculateSpeedMode::PROP;
  }
  if (calibration_in_progress_ && (timer_task_.getScheduleTime() - calibration_start_time_us_ >= TIMEOUT_CALIBRATION)) {
    // Timeout, Kalibrierung abbrechen
    stopCalibration(true);
  } else {
    if (!calibration_pwm_in_progress_) {
      // Erster Durchlauf der Kalibrierung
      Serial.println(F("Erster Durchlauf für Stufe, calibration_pwm_in_progress_"));
      calibration_pwm_in_progress_ = true;
      calibration_pwm_start_time_us_ = timer_task_.getScheduleTime();
      fan1_.prepareCalibration();
      fan2_.prepareCalibration();
    }
    if (calibration_pwm_in_progress_ && (timer_task_.getScheduleTime() - calibration_pwm_start_time_us_ <= TIMEOUT_PWM_CALIBRATION)) {
      // Einzelne Stufen kalibrieren
      if (speedCalibrationPWMStep()) {
        // true = Kalibrierung der Lüftungsstufe beendet
        if (current_calibration_mode_ == KWLConfig::StandardModeCnt - 1) {
          // fertig mit allen Stufen!!!
          // Speichern in EEProm und Variablen
          fan1_.finishCalibration();
          fan2_.finishCalibration();
          storePWMSettingsToEEPROM();
          for (unsigned i = 0; ((i < KWLConfig::StandardModeCnt) && (i < 10)); i++) {
            Serial.print(F("Stufe: "));
            Serial.print(i);
            Serial.print(F("  PWM Fan 1: "));
            Serial.print(fan1_.getPWM(i));
            Serial.print(F("  PWM Fan 2: "));
            Serial.println(fan2_.getPWM(i));
          }
          stopCalibration(false);
        } else {
          // nächste Stufe
          calibration_pwm_in_progress_ = false;
          current_calibration_mode_++;
        }
      }
    } else {
      // one PWM step timed out
      stopCalibration(true);
    }
  }
}

bool FanControl::speedCalibrationPWMStep()
{
  bool r1 = fan1_.speedCalibrationStep(current_calibration_mode_);
  bool r2 = fan2_.speedCalibrationStep(current_calibration_mode_);
  setSpeed();
  return r1 && r2;
}

void FanControl::stopCalibration(bool timeout)
{
  mode_ = FanMode::Normal;
  calibration_in_progress_ = false;
  calibration_start_time_us_ = 0;
  if (timeout)
    Serial.println(F("Error: Kalibrierung NICHT erfolgreich"));
  else
    Serial.println(F("Kalibrierung erfolgreich beendet"));
  // TODO shouldn't this also reset fan speed immediately?
}

void FanControl::storePWMSettingsToEEPROM()
{
  for (unsigned i = 0; ((i < KWLConfig::StandardModeCnt) && (i < 10)); i++) {
    persistent_config_.setFanPWMSetpoint(0, i, fan1_.getPWM(i));
    persistent_config_.setFanPWMSetpoint(1, i, fan2_.getPWM(i));
  }
}

bool FanControl::mqttReceiveMsg(const StringView& topic, const StringView& s)
{
  if (topic == MQTTTopic::CmdFan1Speed) {
    // Drehzahl Lüfter 1
    unsigned i = unsigned(s.toInt());
    getFan1().setStandardSpeed(i);
    persistent_config_.setSpeedSetpointFan1(i);
  } else if (topic == MQTTTopic::CmdFan2Speed) {
    // Drehzahl Lüfter 2
    unsigned i = unsigned(s.toInt());
    getFan2().setStandardSpeed(i);
    persistent_config_.setSpeedSetpointFan2(i);
  } else if (topic == MQTTTopic::CmdMode) {
    // KWL Stufe
    setVentilationMode(int(s.toInt()));
  } else if (topic == MQTTTopic::CmdFansCalculateSpeedMode) {
    if (s == F("PROP"))
      setCalculateSpeedMode(FanCalculateSpeedMode::PROP);
    else if (s == F("PID"))
      setCalculateSpeedMode(FanCalculateSpeedMode::PID);
  } else if (topic == MQTTTopic::CmdCalibrateFans) {
    if (s == F("YES"))
      speedCalibrationStart();
  } else if (topic == MQTTTopic::CmdGetSpeed) {
    forceSend();
#ifdef DEBUG
  } else if (topic == MQTTTopic::KwlDebugsetFan1Getvalues) {
    if (s == F("on"))
      fan1_.debug(true);
    else if (s == F("off"))
      fan1_.debug(false);
  } else if (topic == MQTTTopic::KwlDebugsetFan2Getvalues) {
    if (s == F("on"))
      fan2_.debug(true);
    else if (s == F("off"))
      fan2_.debug(false);
  } else if (topic == MQTTTopic::KwlDebugsetFan1PWM) {
    // update PWM value for the current state
    if (ventilation_mode_ != 0) {
      int value = int(s.toInt());
      fan1_.debugSet(ventilation_mode_, value);
      speedUpdate();
    }
  } else if (topic == MQTTTopic::KwlDebugsetFan2PWM) {
    // update PWM value for the current state
    if (ventilation_mode_ != 0) {
      int value = int(s.toInt());
      fan2_.debugSet(ventilation_mode_, value);
      speedUpdate();
    }
  } else if (topic == MQTTTopic::KwlDebugsetFanPWMStore) {
    // store calibration data in EEPROM
    storePWMSettingsToEEPROM();
#endif
  } else {
    return false;
  }
  return true;
}

void FanControl::sendMQTT()
{
  int fan1 = int(fan1_.getSpeed());
  int fan2 = int(fan2_.getSpeed());
  last_sent_fan1_speed_ = fan1;
  last_sent_fan2_speed_ = fan2;
  auto mode = ventilation_mode_;
  mqtt_publish_.publish([this, fan1, fan2, mode]() {
    if (!publish_if(mqtt_send_flags_, MQTT_SEND_MODE, MQTTTopic::StateKwlMode, mode, KWLConfig::RetainFanMode))
      return false;
    if (!publish_if(mqtt_send_flags_, MQTT_SEND_FAN1, MQTTTopic::Fan1Speed, fan1, KWLConfig::RetainFanSpeed))
      return false;
    if (!publish_if(mqtt_send_flags_, MQTT_SEND_FAN2, MQTTTopic::Fan2Speed, fan2, KWLConfig::RetainFanSpeed))
      return false;
    return true;  // all done
  });
}
