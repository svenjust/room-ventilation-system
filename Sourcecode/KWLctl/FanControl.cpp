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
#include "kwl_config.h"

#include <Arduino.h>
#include <EEPROM.h>

static void eeprom_write_int(unsigned int addr, int value) {
  // TODO use EEPROM config when separated out
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
  EEPROM.write(addr, data[0]);
  EEPROM.write(addr, data[1]);
};

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


FanControl::FanControl(Scheduler& sched, void (*speedCallback)(), Print& initTrace) :
  InitTrace(F("Initialisierung Ventilatoren"), initTrace),
  fan1_(kwl_config::PinFan1Power, kwl_config::PinFan1PWM, kwl_config::PinFan1Tacho, kwl_config::StandardSpeedSetpointFan1, countUpFan1),
  fan2_(kwl_config::PinFan2Power, kwl_config::PinFan2PWM, kwl_config::PinFan2Tacho, kwl_config::StandardSpeedSetpointFan1, countUpFan2),
  speed_callback_(speedCallback),
  ventilation_mode_(kwl_config::StandardKwlMode)
{
  instance_ = this;
  sched.addRepeated(*this, FAN_INTERVAL);
}

void FanControl::setVentilationMode(int mode)
{
  ventilation_mode_ = constrain(mode, 0, int(kwl_config::StandardModeCnt - 1));
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

  if (kwl_config::serialDebugFan == 1) {
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
    if (publish(MQTTTopic::StateKwlMode, ventilation_mode_, kwl_config::RetainFanMode)) {
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
      auto r1 = publish(MQTTTopic::Fan1Speed, fan1, kwl_config::RetainFanSpeed);
      auto r2 = publish(MQTTTopic::Fan2Speed, fan2, kwl_config::RetainFanSpeed);
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
    speed_callback_();

  setSpeed();
}

void FanControl::setSpeed()
{
  fan1_.sendMQTTDebug(1, getScheduleTime(), *this);
  fan2_.sendMQTTDebug(2, getScheduleTime(), *this);

  if (kwl_config::serialDebugFan == 1) {
    Serial.print(F("Timestamp: "));
    Serial.println(getScheduleTime());
  }
  fan1_.setSpeed(1, kwl_config::PinFan1PWM, kwl_config::DacChannelFan1);
  fan2_.setSpeed(2, kwl_config::PinFan2PWM, kwl_config::DacChannelFan2);
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
        if (current_calibration_mode_ == kwl_config::StandardModeCnt - 1) {
          // fertig mit allen Stufen!!!
          // Speichern in EEProm und Variablen
          fan1_.finishCalibration();
          fan2_.finishCalibration();
          storePWMSettingsToEEPROM();
          for (unsigned i = 0; ((i < kwl_config::StandardModeCnt) && (i < 10)); i++) {
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
  for (unsigned i = 0; ((i < kwl_config::StandardModeCnt) && (i < 10)); i++) {
    eeprom_write_int(20 + (i * 4), fan1_.getPWM(i));
    eeprom_write_int(22 + (i * 4), fan2_.getPWM(i));
  }
}

bool FanControl::mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length)
{
  const StringView s(payload, length);
  if (topic == MQTTTopic::CmdFan1Speed) {
    // Drehzahl Lüfter 1
    unsigned i = unsigned(s.toInt());
    getFan1().setStandardSpeed(i);
    eeprom_write_int(2, i);
  } else if (topic == MQTTTopic::CmdFan2Speed) {
    // Drehzahl Lüfter 2
    unsigned i = unsigned(s.toInt());
    getFan2().setStandardSpeed(i);
    eeprom_write_int(4, i);
  } else if (topic == MQTTTopic::CmdMode) {
    // KWL Stufe
    setVentilationMode(int(s.toInt()));
#ifdef DEBUG
  } else if (topic == MQTTTopic::KwlDebugsetFan1Getvalues) {
    if (s == "on")
      fan1_.debug(true);
    else if (s == "off")
      fan1_.debug(false);
  } else if (topic == MQTTTopic::KwlDebugsetFan2Getvalues) {
    if (s == "on")
      fan2_.debug(true);
    else if (s == "off")
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
