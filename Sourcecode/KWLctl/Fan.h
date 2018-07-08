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

/*!
 * @file
 * @brief One fan handler.
 */
#pragma once

#include "FanRPM.h"
#include "Relay.h"
#include "KWLConfig.h"

#include <PID_v1.h>

class MessageHandler;

/// Fan speed calculation mode.
enum class FanCalculateSpeedMode : int8_t
{
  PID = 1,          ///< Use PID regulator (calibration and continuous calibration).
  PROP = 0,         ///< Use simple proportional calculation to PWM signal (normal operation).
  UNSET = -1        ///< Not set.
};

/// One fan handler.
class Fan
{
public:
  /*!
   * @brief Construct one fan interface.
   *
   * @param id fan ID for display purposes (1 or 2).
   * @param powerPin pin on which to drive power relay.
   * @param pwmPin pin on which to send PWM signal.
   * @param tachoPin pin on which to read tacho pulses.
   */
  Fan(uint8_t id, uint8_t powerPin, uint8_t pwmPin, uint8_t tachoPin);

  /*!
   * @brief Start the fan.
   *
   * @param countUp interrupt routine which calls interrupt() for this fan to count RPM.
   * @param standardSpeed standard speed to use initially (in RPM).
   */
  void begin(void (*countUp)(), unsigned standardSpeed);

  /// Get current speed (RPM) of this fan.
  inline unsigned getSpeed() const { return unsigned(current_speed_); }

  /// Get speed (RPM) for the standard ventilation mode.
  inline unsigned getStandardSpeed() const { return standard_speed_; }

  /// Set speed (RPM) for the standard ventilation mode.
  inline void setStandardSpeed(unsigned speed) { standard_speed_ = speed; }

  /// Check whether the fan is set off.
  inline bool isOff() const { return abs(tech_setpoint_) < 0.1; }

  /// Set the fan to off (until next computation).
  inline void off() { tech_setpoint_ = 0; }

  /// Get PWM signal strength for given ventilation mode.
  inline int getPWM(unsigned mode) const { return pwm_setpoint_[mode]; }

  /// Set PWM signal strength for given ventilation mode at initialization time.
  inline void initPWM(unsigned mode, int pwm) { pwm_setpoint_[mode] = pwm; }

private:
  friend class FanControl;

  /// Called by the interrupt routine for this fan.
  inline void interrupt() { rpm_.interrupt(); }

  /// Called by the fan control to capture current RPM measurement.
  inline void capture() { rpm_.capture(); }

  /// Called by the fan control to update speed from RPM measurement.
  inline void updateSpeed() { current_speed_ = rpm_.getSpeed(); }

  /// Update fan speed based on modes.
  void computeSpeed(int ventMode, FanCalculateSpeedMode calcMode);

  /// Set computed fan speed via PWM pin and/or DAC.
  void setSpeed(int id, uint8_t pwmPin, uint8_t dacChannel);

  /// Debug: set PWM signal explicitly for debugging purposes.
  void debugSet(int ventMode, int techSetpoint);

  /// Prepare for calibration.
  void prepareCalibration() { good_pwm_setpoint_count_ = 0; }

  /// Perform one speed calibration step for given mode.
  bool speedCalibrationStep(int mode);

  /// Finish calibration and copy temp PWM values to real PWM values.
  void finishCalibration();

  /// Set debugging mode for this fan via MQTT.
  inline void debug(bool on) { mqtt_send_debug_ = on; }

  /// Send MQTT debugging message, if on.
  void sendMQTTDebug(int id, unsigned long ts, MessageHandler& h);

  /// How many "good" measurements do we need to consider the calibration good.
  static constexpr unsigned REQUIRED_GOOD_PWM_COUNT = 30;

  FanRPM rpm_;  ///< Speed measurement and setting.
  Relay power_; ///< Power relay.

  double current_speed_ = 0;            ///< Current speed of the fan in RPM.
  double speed_setpoint_ = 0;           ///< Desired speed of the fan in RPM.
  double tech_setpoint_ = 0;            ///< Needed PWM signal to set this fan speed.
  unsigned standard_speed_ = 0;         ///< Standard speed of this fan (configuration for default ventilation mode).
  int pwm_setpoint_[MAX_FAN_MODE_CNT];  ///< Current set of PWM output for ventilation modes.
  int calibration_pwm_setpoint_[MAX_FAN_MODE_CNT];  ///< Temporary PWM values during calibration.
  int good_pwm_setpoint_[REQUIRED_GOOD_PWM_COUNT];  ///< PWM signal strength considered "good" during calibration.
  unsigned good_pwm_setpoint_count_ = 0;            ///< # of "good" PWM signal strengths we already know.
  bool mqtt_send_debug_ = false;        ///< Send debugging info for this fan per MQTT.
  uint8_t pwm_pin_;                     ///< Pin to send PWM signa to.
  uint8_t tacho_pin_;                   ///< Pin to read tacho signal from.
  uint8_t fan_id_;                      ///< Fan ID (1 or 2).
  PID pid_;                             ///< PID regulator for this fan.
};

