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
 * @brief Fan regulation and status reporting.
 */
#pragma once

#include "Scheduler.h"
#include "Fan.h"
#include "NetworkClient.h"

class Print;
class KWLPersistentConfig;

/// Fan operation modes.
enum class FanMode : uint8_t
{
  Normal = 0,       ///< Normal operation.
  Calibration = 1   ///< Calibration in progress.
};

/*!
 * @brief Fan regulation and status reporting.
 */
class FanControl : private Task, private MessageHandler
{
public:
  FanControl(const FanControl&) = delete;
  FanControl& operator==(const FanControl&) = delete;

  /// Callback called when fan speed is set.
  class SetSpeedCallback {
  public:
    /// Callback called when fan speed is set.
    virtual void fanSpeedSet() = 0;
    virtual ~SetSpeedCallback() {}
  };

  /*!
   * @brief Construct fan control object.
   *
   * @param config configuration to read/write.
   * @param speedCallback callback to call after computing PWM signal for fans, but before
   *        setting it. Typically used for antifreeze/preheater regulation and to turn off
   *        fans if no preheater installed.
   */
  explicit FanControl(KWLPersistentConfig& config, SetSpeedCallback *speedCallback);

  /*!
   * @brief Start fans.
   *
   * @param sched scheduler to use for scheduling events.
   * @param initTrace initial tracer for printing startup messages.
   */
  void start(Scheduler& sched, Print& initTrace);

  /// Get current fan mode (normal or calibration).
  inline FanMode getMode() { return mode_; }

  /// Get current mode (0=off, others the current ventilation mode).
  inline int getVentilationMode() { return ventilation_mode_; }

  /// Set current mode (0=off).
  void setVentilationMode(int mode);

  /// Get mode of fan speed calculation.
  FanCalculateSpeedMode getCalculateSpeedMode() { return calc_speed_mode_; }

  /// Set mode of fan speed calculation.
  void setCalculateSpeedMode(FanCalculateSpeedMode mode) { calc_speed_mode_ = mode; }

  /// Get interface of fan 1 (intake).
  inline Fan& getFan1() { return fan1_; }

  /// Get interface of fan 2 (exhaust)
  inline Fan& getFan2() { return fan2_; }

  /// Force sending mode message via MQTT independent of timing.
  inline void forceSendMode() { force_send_mode_ = true; }

  /// Force sending speed message via MQTT independent of timing.
  inline void forceSendSpeed() { force_send_speed_ = true; }

  /// Starts speed calibration.
  void speedCalibrationStart();

  /// Get current ventilation mode for which the calibration runs.
  inline int getVentilationCalibrationMode() { return current_calibration_mode_; }

private:
  static void countUpFan1();
  static void countUpFan2();

  virtual void run() override;

  /// Sets fan speed based on ventilation mode.
  void speedUpdate();

  /// Sets fan speed based currently-set PWM signal strength.
  void setSpeed();

  /// Called to process the next calibration step.
  void speedCalibrationStep();

  /// Called to process the next calibration step for one PWM step.
  bool speedCalibrationPWMStep();

  /// Called to end/cancel calibration.
  void stopCalibration(bool timeout);

  /// Save current PWM settings to EEPROM.
  void storePWMSettingsToEEPROM();

  virtual bool mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length) override;

  Fan fan1_;   ///< Control for fan 1 (intake).
  Fan fan2_;   ///< Control for fan 2 (exhaust).

  SetSpeedCallback *speed_callback_;///< Callback to call when new tech points for fans computed.
  int ventilation_mode_;            ///< Current ventilation mode (0-n).
  FanMode mode_ = FanMode::Normal;  ///< Current operation mode.
  FanCalculateSpeedMode calc_speed_mode_ = FanCalculateSpeedMode::PROP;

  bool calibration_in_progress_ = false;        ///< Flag set during calibration.
  bool calibration_pwm_in_progress_ = false;    ///< Flag set during calibration of one PWM mode.
  int current_calibration_mode_ = 0;            ///< Current mode being calibrated.
  unsigned long calibration_start_time_us_ = 0; ///< Start of calibration.
  unsigned long calibration_pwm_start_time_us_ = 0; ///< Start of one PWM mode calibration.

  KWLPersistentConfig& persistent_config_;      ///< Configuration.

  bool force_send_mode_ = false;    ///< Force sending mode on the next run.
  bool force_send_speed_ = false;   ///< Force sending speed on the next run.
  int send_mode_countdown_ = 0;     ///< Countdown until sending mode (in run intervals).
  int send_fan_countdown_ = 0;      ///< Countdown until sending fan state (in run intervals).
  int send_fan_oversampling_countdown_ = 0; ///< Countdown until sending fan state unconditionally.
  int last_sent_fan1_speed_ = 0;    ///< Last reported fan 1 speed.
  int last_sent_fan2_speed_ = 0;    ///< Last reported fan 2 speed.
};
