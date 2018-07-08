/*
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
#include "KWLPersistentConfig.h"
#include "KWLConfig.h"

#include <Arduino.h>

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


FanControl::FanControl(KWLPersistentConfig& config, SetSpeedCallback *speedCallback) :
  Task(F("FanControl")),
  fan1_(1, KWLConfig::PinFan1Power, KWLConfig::PinFan1PWM, KWLConfig::PinFan1Tacho),
  fan2_(2, KWLConfig::PinFan2Power, KWLConfig::PinFan2PWM, KWLConfig::PinFan2Tacho),
  speed_callback_(speedCallback),
  ventilation_mode_(KWLConfig::StandardKwlMode),
  persistent_config_(config)
{}

void FanControl::begin(Scheduler& sched, Print& initTrace)
{
  initTrace.println(F("Initialisierung Ventilatoren"));

  instance_ = this;
  for (unsigned i = 0; ((i < KWLConfig::StandardModeCnt) && (i < 10)); i++) {
    fan1_.initPWM(i, persistent_config_.getFanPWMSetpoint(0, i));
    fan2_.initPWM(i, persistent_config_.getFanPWMSetpoint(1, i));
  }
  fan1_.begin(countUpFan1, persistent_config_.getSpeedSetpointFan1());
  fan2_.begin(countUpFan2, persistent_config_.getSpeedSetpointFan2());

  sched.addRepeated(*this, FAN_INTERVAL);
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

  noInterrupts();
  fan1_.capture();
  fan2_.capture();
  interrupts();

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
  if (force_send_mode_ || --send_mode_countdown_ <= 0) {
    if (publish(MQTTTopic::StateKwlMode, ventilation_mode_, KWLConfig::RetainFanMode)) {
      send_mode_countdown_ = int(MODE_MQTT_INTERVAL / FAN_INTERVAL);
      force_send_mode_ = false;
    } else {
      force_send_mode_ = true;  // retry next time on failure
    }
  }
  if (--send_fan_oversampling_countdown_ <= 0)
    force_send_speed_ = true;
  if (force_send_speed_ || --send_fan_countdown_ <= 0) {
    int fan1 = int(fan1_.getSpeed());
    int fan2 = int(fan2_.getSpeed());
    if (!force_send_speed_) {
      // check whether we need to send data
      if (abs(fan1 - last_sent_fan1_speed_) >= MIN_SPEED_DIFF ||
          abs(fan2 - last_sent_fan2_speed_) >= MIN_SPEED_DIFF) {
        force_send_speed_ = true;
      } else {
        send_fan_countdown_ = int(FAN_MQTT_INTERVAL / FAN_INTERVAL);
      }
    }
    if (force_send_speed_) {
      // yep, really sending now
      auto r1 = publish(MQTTTopic::Fan1Speed, fan1, KWLConfig::RetainFanSpeed);
      auto r2 = publish(MQTTTopic::Fan2Speed, fan2, KWLConfig::RetainFanSpeed);
      last_sent_fan1_speed_ = fan1;
      last_sent_fan2_speed_ = fan2;
      if (r1 && r2) {
        // succeeded
        force_send_speed_ = false;
        send_fan_countdown_ = int(FAN_MQTT_INTERVAL / FAN_INTERVAL);
        send_fan_oversampling_countdown_ = int(FAN_MQTT_INTERVAL_OVERSAMPLING / FAN_INTERVAL);
      } // else force send stays set and will retry next time
    }
  }
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
  fan1_.sendMQTTDebug(1, getScheduleTime(), *this);
  fan2_.sendMQTTDebug(2, getScheduleTime(), *this);

  if (KWLConfig::serialDebugFan) {
    Serial.print(F("Timestamp: "));
    Serial.println(getScheduleTime());
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
    calibration_start_time_us_ = getScheduleTime();
    current_calibration_mode_ = 0;
    calc_speed_mode_ = FanCalculateSpeedMode::PROP;
  }
  if (calibration_in_progress_ && (getScheduleTime() - calibration_start_time_us_ >= TIMEOUT_CALIBRATION)) {
    // Timeout, Kalibrierung abbrechen
    stopCalibration(true);
  } else {
    if (!calibration_pwm_in_progress_) {
      // Erster Durchlauf der Kalibrierung
      Serial.println ("Erster Durchlauf für Stufe, calibration_pwm_in_progress_");
      calibration_pwm_in_progress_ = true;
      calibration_pwm_start_time_us_ = getScheduleTime();
      fan1_.prepareCalibration();
      fan2_.prepareCalibration();
    }
    if (calibration_pwm_in_progress_ && (getScheduleTime() - calibration_pwm_start_time_us_ <= TIMEOUT_PWM_CALIBRATION)) {
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

bool FanControl::mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length)
{
  const StringView s(payload, length);
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
    forceSendSpeed();
    forceSendMode();
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
      forceSendSpeed();
    }
  } else if (topic == MQTTTopic::KwlDebugsetFan2PWM) {
    // update PWM value for the current state
    if (ventilation_mode_ != 0) {
      int value = int(s.toInt());
      fan2_.debugSet(ventilation_mode_, value);
      speedUpdate();
      forceSendSpeed();
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
