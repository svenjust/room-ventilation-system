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
#include "MQTTTopic.hpp"
#include "StringView.h"
#include "TempSensors.h"
#include "KWLConfig.h"

/// Check bypass every 20s (also terminates motor running, if needed).
static constexpr unsigned long INTERVAL_BYPASS_CHECK = 20000000;

/// Interval for sending MQTT messages when nothing changes (15 min).
static constexpr unsigned long INTERVAL_MQTT_BYPASS_STATE = 900000000UL;

/// Runtime of the bypass motor in ms (2 minutes).
static constexpr unsigned long BYPASS_FLAPS_DRIVE_TIME = 120 * 1000000UL;

/// Maximum hysteresis temperature, which makes sense.
static constexpr long MAX_TEMP_HYSTERESIS = 10;

SummerBypass::SummerBypass(KWLPersistentConfig& config, const TempSensors& temp) :
  MessageHandler(F("SummerBypass")),
  config_(config),
  temp_(temp),
  rel_bypass_power_(KWLConfig::PinBypassPower),
  rel_bypass_direction_(KWLConfig::PinBypassDirection),
  stats_(F("SummerBypass")),
  timer_task_(stats_, &SummerBypass::run, *this)
{}

void SummerBypass::begin(Print& initTrace)
{
  initTrace.println(F("Initialisierung Sommer-Bypass"));

  // Relais Ansteuerung Bypass
  rel_bypass_power_.off();
  rel_bypass_direction_.off();

  timer_task_.runRepeated(INTERVAL_BYPASS_CHECK);

  if (KWLConfig::RetainBypassConfigState)
    sendMQTT(true);
}

void SummerBypass::forceSend(bool all_values)
{
  mqtt_countdown_ = 0;
  sendMQTT(all_values);
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
      if (KWLConfig::serialDebugSummerbypass)
        Serial.print(F(" motor off; flap now "));
      bypass_motor_running_ = false;
    } else {
      // should never get here, we'll retry
      if (KWLConfig::serialDebugSummerbypass)
        Serial.print(F(" motor running; flap going to "));
    }
    if (KWLConfig::serialDebugSummerbypass)
      Serial.println(toString(flap_setpoint_));
    sendMQTT();
    timer_task_.setInterval(INTERVAL_BYPASS_CHECK);
    return;
  }

  // normal operation, motor is not running
  bool changed = false;
  if (config_.getBypassMode() == SummerBypassMode::AUTO) {
    // Automatic - first compute desired state based on current values
    SummerBypassFlapState desired_setpoint = SummerBypassFlapState::UNKNOWN;
    if ((temp_.get_t1_outside() > TempSensors::INVALID)
        && (temp_.get_t3_outlet() > TempSensors::INVALID)) {
      if ((temp_.get_t1_outside() < temp_.get_t3_outlet() - config_.getBypassHysteresisTemp())  // TODO configurable
          && (temp_.get_t3_outlet() > config_.getBypassTempAbluftMin())
          && (temp_.get_t1_outside() > config_.getBypassTempAussenluftMin())) {
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
      Serial.print(F(" auto check T1="));
      Serial.print(temp_.get_t1_outside());
      Serial.print('>');
      Serial.print(config_.getBypassTempAussenluftMin());
      Serial.print(F(" && T3="));
      Serial.print(temp_.get_t3_outlet());
      Serial.print('>');
      Serial.print(config_.getBypassTempAbluftMin());
      Serial.print(F(" && T3-T1>"));
      Serial.print(config_.getBypassHysteresisTemp());
      Serial.print(F(", desired state: "));
      Serial.print(toString(desired_setpoint));
    }

    if (desired_setpoint != SummerBypassFlapState::UNKNOWN && desired_setpoint != flap_setpoint_) {
      // we have a change request, see if really changeable
      auto current_time = millis();
      if ((current_time - last_change_time_millis_ >= (config_.getBypassHystereseMinutes() * 60L * 1000L))
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
    if (config_.getBypassManualSetpoint() != flap_setpoint_) {
      flap_setpoint_ = config_.getBypassManualSetpoint();
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
    timer_task_.setInterval(BYPASS_FLAPS_DRIVE_TIME);
  } else {
    if (KWLConfig::serialDebugSummerbypass)
      Serial.println(F(" no change"));
    timer_task_.setInterval(INTERVAL_BYPASS_CHECK);
  }
  if (--mqtt_countdown_ <= 0 || mqtt_state_ != state_) {
    sendMQTT();
  }
}

bool SummerBypass::mqttReceiveMsg(const StringView& topic, const StringView& s)
{
  if (topic == MQTTTopic::CmdBypassGetValues) {
    forceSend(true);
  } else if (topic == MQTTTopic::CmdBypassHystereseMinutes) {
    unsigned i = unsigned(s.toInt());
    config_.setBypassHystereseMinutes(i);
  } else if (topic == MQTTTopic::CmdBypassManualFlap) {
    if (s == F("open"))
      config_.setBypassManualSetpoint(SummerBypassFlapState::OPEN);
    if (s == F("close"))
      config_.setBypassManualSetpoint(SummerBypassFlapState::CLOSED);
    // Stellung Bypassklappe bei manuellem Modus
  } else if (topic == MQTTTopic::CmdBypassMode) {
    // Auto oder manueller Modus
    if (s == F("auto"))   {
      config_.setBypassMode(SummerBypassMode::AUTO);
      forceSend();
    } else if (s == F("manual")) {
      config_.setBypassMode(SummerBypassMode::USER);
      forceSend();
    }
  } else if (topic == MQTTTopic::CmdBypassHyst) {
      auto i = s.toInt();
      if (i < 0)
        i = 0;
      if (i > MAX_TEMP_HYSTERESIS)
        i = MAX_TEMP_HYSTERESIS;
      config_.setBypassHysteresisTemp(uint8_t(i));
      forceSend(true);
  } else if (topic == MQTTTopic::CmdBypassTempAbluftMin) {
      auto i = s.toInt();
      if (i < 0)
        i = 0;
      config_.setBypassTempAbluftMin(unsigned(i));
      forceSend(true);
  } else if (topic == MQTTTopic::CmdBypassTempAussenluftMin) {
      auto i = s.toInt();
      if (i < 0)
        i = 0;
      config_.setBypassTempAussenluftMin(unsigned(i));
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

void SummerBypass::sendMQTT(bool all_values)
{
  mqtt_countdown_ = INTERVAL_MQTT_BYPASS_STATE / INTERVAL_BYPASS_CHECK;
  mqtt_state_ = state_;

  uint8_t bitmask = all_values ? 31 : 1;
  publish_task_.publish([this, bitmask]() mutable {
    if (!publish_if(bitmask, uint8_t(1), MQTTTopic::KwlBypassState, toString(mqtt_state_), KWLConfig::RetainBypassState))
      return false;
    if (!publish_if(bitmask, uint8_t(2), MQTTTopic::KwlBypassMode,
                    (config_.getBypassMode() == SummerBypassMode::AUTO) ? F("auto") : F("manual"),
                    KWLConfig::RetainBypassConfigState))
      return false;
    if (!publish_if(bitmask, uint8_t(4), MQTTTopic::KwlBypassTempAbluftMin, config_.getBypassTempAbluftMin(), KWLConfig::RetainBypassConfigState))
      return false;
    if (!publish_if(bitmask, uint8_t(8), MQTTTopic::KwlBypassTempAussenluftMin, config_.getBypassTempAussenluftMin(), KWLConfig::RetainBypassConfigState))
      return false;
    if (!publish_if(bitmask, uint8_t(16), MQTTTopic::KwlBypassHystereseMinutes, config_.getBypassHystereseMinutes(), KWLConfig::RetainBypassConfigState))
      return false;
    return true;
  });
}
