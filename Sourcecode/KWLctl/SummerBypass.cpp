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

#include "SummerBypass.h"
#include "Scheduler.h"
#include "MQTTTopic.hpp"
#include "StringView.h"
#include "TempSensors.h"
#include "KWLConfig.h"
#include "KWLPersistentConfig.h"

/// Check bypass every 20s (also terminates motor running, if needed).
static constexpr unsigned long INTERVAL_BYPASS_CHECK = 20000000;

/// Interval for sending MQTT messages when nothing changes (15 min).
static constexpr unsigned long INTERVAL_MQTT_BYPASS_STATE = 900000000UL;

/// Runtime of the bypass motor in ms (2 minutes).
static constexpr unsigned long BYPASS_FLAPS_DRIVE_TIME = 120 * 1000000UL;

/// Maximum hysteresis temperature, which makes sense.
static constexpr long MAX_TEMP_HYSTERESIS = 10;

SummerBypass::SummerBypass(Scheduler& sched, KWLPersistentConfig& config, const TempSensors& temp) :
  Task(F("SummerBypass")),
  scheduler_(sched),
  config_(config),
  temp_(temp),
  rel_bypass_power_(KWLConfig::PinBypassPower),
  rel_bypass_direction_(KWLConfig::PinBypassDirection),
  bypass_mode_(SummerBypassMode(KWLConfig::StandardBypassMode)),
  temp_outlet_min_(KWLConfig::StandardBypassTempAbluftMin),
  temp_outside_min_(KWLConfig::StandardBypassTempAussenluftMin),
  temp_diff_min_(KWLConfig::StandardBypassHysteresisTemp),
  manual_flap_setpoint_(SummerBypassFlapState(KWLConfig::StandardBypassManualSetpoint)),  // Standardstellung Bypass
  hysteresis_minutes_(KWLConfig::StandardBypassHystereseMinutes),
  mqtt_send_all_(KWLConfig::RetainBypassConfigState)
{}

void SummerBypass::begin(Print& initTrace)
{
  initTrace.println(F("Initializing summer bypass"));

  temp_outlet_min_ = config_.getBypassTempAbluftMin();
  temp_outside_min_ = config_.getBypassTempAussenluftMin();
  temp_diff_min_ = config_.getBypassHysteresisTemp();
  hysteresis_minutes_ = config_.getBypassHystereseMinutes();
  manual_flap_setpoint_ = SummerBypassFlapState(config_.getBypassManualSetpoint());
  bypass_mode_ = SummerBypassMode(config_.getBypassMode());

  // Relais Ansteuerung Bypass
  rel_bypass_power_.off();
  rel_bypass_direction_.off();

  scheduler_.addRepeated(*this, INTERVAL_BYPASS_CHECK);
}

const __FlashStringHelper* SummerBypass::toString(SummerBypassFlapState state)
{
  switch (state) {
  case SummerBypassFlapState::OPEN:
    return F("open");
  case SummerBypassFlapState::CLOSED:
    return F("closed");
  default:
    return F("unknown");
  }
}

void SummerBypass::run()
{
  // Bedingungen für Sommer Bypass überprüfen und Variable ggfs setzen
  if (KWLConfig::serialDebugSummerbypass) {
    Serial.print(F("BYPASS: state "));
    Serial.print(toString(flap_setpoint_));
  }
  if (bypass_motor_running_) {
    // check whether we are done with running the motor (just to be on the safe side, should be)
    if (millis() - last_change_time_millis_ >= (BYPASS_FLAPS_DRIVE_TIME / 1000) - 1000) {
      // done moving, turn off relays
      rel_bypass_power_.off();
      rel_bypass_direction_.off();
      state_ = flap_setpoint_;
      Serial.print(F(" motor off; flap now "));
      bypass_motor_running_ = false;
    } else {
      // should never get here, we'll retry
      Serial.print(F(" motor running; flap going to "));
    }
    Serial.println(toString(flap_setpoint_));
    sendMqtt();
    setInterval(INTERVAL_BYPASS_CHECK);
    return;
  }

  // normal operation, motor is not running
  bool changed = false;
  if (bypass_mode_ == SummerBypassMode::AUTO) {
    // Automatic - first compute desired state based on current values
    SummerBypassFlapState desired_setpoint = SummerBypassFlapState::UNKNOWN;
    if ((temp_.get_t1_outside() > TempSensors::INVALID)
        && (temp_.get_t3_outlet() > TempSensors::INVALID)) {
      if ((temp_.get_t1_outside() < temp_.get_t3_outlet() - temp_diff_min_)  // TODO configurable
          && (temp_.get_t3_outlet() > temp_outlet_min_)
          && (temp_.get_t1_outside() > temp_outside_min_)) {
        //ok, dann Klappe öffen
        desired_setpoint = SummerBypassFlapState::OPEN;
      } else {
        //ok, dann Klappe schliessen
        desired_setpoint = SummerBypassFlapState::CLOSED;
      }
    } else {
      if (KWLConfig::serialDebugSummerbypass)
        Serial.print(F(" T1/T3 SENSOR ERROR"));
    }
    if (KWLConfig::serialDebugSummerbypass) {
      Serial.print(F(" auto check T1_outside="));
      Serial.print(temp_.get_t1_outside());
      Serial.print('>');
      Serial.print(temp_outside_min_);
      Serial.print(F(" && T3_outlet="));
      Serial.print(temp_.get_t3_outlet());
      Serial.print('>');
      Serial.print(temp_outlet_min_);
      Serial.print(F(" && T3-T1>"));
      Serial.print(temp_diff_min_);
      Serial.print(F(", desired state: "));
      Serial.print(toString(desired_setpoint));
    }

    if (desired_setpoint != SummerBypassFlapState::UNKNOWN && desired_setpoint != flap_setpoint_) {
      // we have a change request, see if really changeable
      auto current_time = millis();
      if ((current_time - last_change_time_millis_ >= (hysteresis_minutes_ * 60L * 1000L))
          || (state_ == SummerBypassFlapState::UNKNOWN)) {
        flap_setpoint_ = desired_setpoint;
        changed = true;
      } else {
        if (KWLConfig::serialDebugSummerbypass)
          Serial.print(F(" hysteresis"));
      }
    }
  } else {
    // Manuelle Schaltung
    if (manual_flap_setpoint_ != flap_setpoint_) {
      flap_setpoint_ = manual_flap_setpoint_;
      if (KWLConfig::serialDebugSummerbypass)
        Serial.print(F(" manual"));
      changed = true;
    }
  }

  if (changed) {
    if (KWLConfig::serialDebugSummerbypass) {
      Serial.print(F(" change to "));
      Serial.println(toString(flap_setpoint_));
    }
    // TODO start the motor, send MQTT message
    last_change_time_millis_ = millis();
    startMoveFlap();
    setInterval(BYPASS_FLAPS_DRIVE_TIME);
  } else {
    if (KWLConfig::serialDebugSummerbypass)
      Serial.println(F(" no change"));
    setInterval(INTERVAL_BYPASS_CHECK);
  }
  sendMqtt();
}

bool SummerBypass::mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length)
{
  StringView s(payload, length);
  if (topic == MQTTTopic::CmdBypassGetValues) {
    forceSend(true);
  } else if (topic == MQTTTopic::CmdBypassHystereseMinutes) {
    unsigned i = unsigned(s.toInt());
    hysteresis_minutes_ = i;
    config_.setBypassHystereseMinutes(i);
  } else if (topic == MQTTTopic::CmdBypassManualFlap) {
    if (s == F("open"))
      manual_flap_setpoint_ = SummerBypassFlapState::OPEN;
    if (s == F("close"))
      manual_flap_setpoint_ = SummerBypassFlapState::CLOSED;
    // Stellung Bypassklappe bei manuellem Modus
  } else if (topic == MQTTTopic::CmdBypassMode) {
    // Auto oder manueller Modus
    if (s == F("auto"))   {
      bypass_mode_ = SummerBypassMode::AUTO;
      forceSend();
    } else if (s == F("manual")) {
      bypass_mode_ = SummerBypassMode::USER;
      forceSend();
    }
  } else if (topic == MQTTTopic::CmdBypassHyst) {
      auto i = s.toInt();
      if (i < 0)
        i = 0;
      if (i > MAX_TEMP_HYSTERESIS)
        i = MAX_TEMP_HYSTERESIS;
      temp_diff_min_ = uint8_t(i);
      config_.setBypassHysteresisTemp(temp_diff_min_);
      forceSend(true);
  } else if (topic == MQTTTopic::CmdBypassTempAbluftMin) {
      auto i = s.toInt();
      if (i < 0)
        i = 0;
      temp_outlet_min_ = unsigned(i);
      config_.setBypassTempAbluftMin(temp_outlet_min_);
      forceSend(true);
  } else if (topic == MQTTTopic::CmdBypassTempAussenluftMin) {
      auto i = s.toInt();
      if (i < 0)
        i = 0;
      temp_outside_min_ = unsigned(i);
      config_.setBypassTempAussenluftMin(temp_outside_min_);
      forceSend(true);
  } else {
    return false;
  }
  return true;
}

void SummerBypass::startMoveFlap()
{
  // Jetzt werden die Relais angesteuert
  // Erst Richtung, dann Power
  if (flap_setpoint_ == SummerBypassFlapState::CLOSED) {
    rel_bypass_direction_.off();
  } else if (flap_setpoint_ == SummerBypassFlapState::OPEN) {
    rel_bypass_direction_.on();
  } else {
    return; // ERROR: invalid value, don't turn on motor
  }
  rel_bypass_power_.on();
  bypass_motor_running_ = true;
}

void SummerBypass::sendMqtt()
{
  if (--mqtt_countdown_ <= 0 || mqtt_state_ != state_) {
    mqtt_state_ = state_;
    if (MessageHandler::publish(MQTTTopic::KwlBypassState, toString(mqtt_state_), KWLConfig::RetainBypassState))
      mqtt_countdown_ = INTERVAL_MQTT_BYPASS_STATE / INTERVAL_BYPASS_CHECK;
    else
      mqtt_countdown_ = 1;  // could not send now, retry next time
  }

  // Senden der Bypass - Einstellung per Mqtt
  // Bedingung: a) mqttCmdSendBypassAllValues == true
  if (mqtt_send_all_) {
    // TODO handle more intelligently
    mqtt_send_all_ = false;

    // TODO what about retain here? probably not, since it's on-request
    if (bypass_mode_ == SummerBypassMode::AUTO) {
      MessageHandler::publish(MQTTTopic::KwlBypassMode, F("auto"), KWLConfig::RetainBypassConfigState);
    } else if (bypass_mode_ == SummerBypassMode::USER) {
      MessageHandler::publish(MQTTTopic::KwlBypassMode, F("manual"), KWLConfig::RetainBypassConfigState);
    }
    MessageHandler::publish(MQTTTopic::KwlBypassTempAbluftMin, temp_outlet_min_, KWLConfig::RetainBypassConfigState);
    MessageHandler::publish(MQTTTopic::KwlBypassTempAussenluftMin, temp_outside_min_, KWLConfig::RetainBypassConfigState);
    MessageHandler::publish(MQTTTopic::KwlBypassHystereseMinutes, hysteresis_minutes_, KWLConfig::RetainBypassConfigState);
  }
}
