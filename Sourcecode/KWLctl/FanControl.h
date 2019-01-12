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

/*!
 * @file
 * @brief Fan regulation and status reporting.
 */
#pragma once

#include "Relay.h"
#include "KWLConfig.h"

#include <FanRPM.h>
#include <TimeScheduler.h>
#include <MessageHandler.h>

#include <PID_v1.h>

class Print;
class KWLPersistentConfig;

class MessageHandler;

/// Fan operation modes.
enum class FanMode : uint8_t
{
  Normal = 0,       ///< Normal operation.
  Calibration = 1   ///< Calibration in progress.
};

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
   * @param multiplier tacho signal frequency multiplier.
   */
  Fan(uint8_t id, uint8_t powerPin, uint8_t pwmPin, uint8_t tachoPin, float multiplier = 1.0);

  /*!
   * @brief Start the fan.
   *
   * @param countUp interrupt routine which calls interrupt() for this fan to count RPM.
   * @param standardSpeed standard speed to use initially (in RPM).
   * @param multiplier tacho signal frequency multiplier.
   */
  void begin(void (*countUp)(), unsigned standardSpeed, float multiplier = 1.0);

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

  /// Set RPM multiplier for this fan.
  void setMultiplier(float multiplier) {
    rpm_.multiplier() = static_cast<FanRPM::multiplier_t>(multiplier * FanRPM::RPM_MULTIPLIER_BASE);
  }
private:
  friend class FanControl;

  /// Called by the interrupt routine for this fan.
  inline void interrupt() { rpm_.interrupt(); }

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

/*!
 * @brief Fan regulation and status reporting.
 */
class FanControl : private MessageHandler
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
   * @param initTrace initial tracer for printing startup messages.
   */
  void begin(Print& initTrace);

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
  inline void forceSendMode() { mqtt_send_flags_ |= MQTT_SEND_MODE; sendMQTT(); }

  /// Force sending speed message via MQTT independent of timing.
  inline void forceSend() { mqtt_send_flags_ |= MQTT_SEND_MODE | MQTT_SEND_FAN1 | MQTT_SEND_FAN2; sendMQTT(); }

  /// Starts speed calibration.
  void speedCalibrationStart();

  /// Get current ventilation mode for which the calibration runs.
  inline int getVentilationCalibrationMode() { return current_calibration_mode_; }

private:
  static void countUpFan1();
  static void countUpFan2();

  void run();

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

  virtual bool mqttReceiveMsg(const StringView& topic, const StringView& s) override;

  /// Send requested messages, if any.
  void sendMQTT();

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

  static constexpr uint8_t MQTT_SEND_MODE = 1;
  static constexpr uint8_t MQTT_SEND_FAN1 = 2;
  static constexpr uint8_t MQTT_SEND_FAN2 = 4;

  int send_mode_countdown_ = 0;     ///< Countdown until sending mode (in run intervals).
  int send_fan_countdown_ = 0;      ///< Countdown until sending fan state (in run intervals).
  int send_fan_oversampling_countdown_ = 0; ///< Countdown until sending fan state unconditionally.
  int last_sent_fan1_speed_ = 0;    ///< Last reported fan 1 speed.
  int last_sent_fan2_speed_ = 0;    ///< Last reported fan 2 speed.
  PublishTask mqtt_publish_;        ///< Task to reliably send values.
  uint8_t mqtt_send_flags_ = 0;     ///< Pending stuff to send.
  Scheduler::TaskTimingStats stats_;            ///< Runtime statistics.
  Scheduler::TimedTask<FanControl> timer_task_; ///< Timer for updating state repeatedly.
};
