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
 * @brief Summer bypass regulation and status reporting.
 */
#pragma once

#include "TimeScheduler.h"
#include "MessageHandler.h"
#include "Relay.h"
#include "KWLConfig.h"

class Print;
class KWLPersistentConfig;
class TempSensors;

/*!
 * @brief Summer bypass regulation and status reporting.
 */
class SummerBypass : private MessageHandler
{
public:
  SummerBypass(const SummerBypass&) = delete;
  SummerBypass& operator==(const SummerBypass&) = delete;

  /*!
   * @brief Construct summer bypass control object.
   *
   * @param config configuration to read/write.
   * @param temp temperature sensors.
   */
  SummerBypass(KWLPersistentConfig& config, const TempSensors& temp);

  /*!
   * @brief Start summer bypass.
   *
   * @param initTrace initial tracer for printing startup messages.
   */
  void begin(Print& initTrace);

  /// Check whether bypass motor is running now.
  bool isRunning() const { return bypass_motor_running_; }

  /// Get current state of the bypass flap.
  SummerBypassFlapState getState() const { return state_; }

  /// Get target state of the bypass flap (while running).
  SummerBypassFlapState getTargetState() const { return flap_setpoint_; }

  /// Force sending state via MQTT.
  void forceSend(bool all_values = false);

  /// Format state.
  static const __FlashStringHelper* toString(SummerBypassFlapState state);

private:
  void run();
  virtual bool mqttReceiveMsg(const StringView& topic, const StringView& s) override;

  /// Start moving the flap to the desired position.
  void startMoveFlap();

  /// Send MQTT message upon change or when timer hits.
  void sendMQTT(bool all_values = false);

  /// Persistent configuration.
  KWLPersistentConfig& config_;
  /// Temperature sensor array.
  const TempSensors& temp_;
  /// Relay for bypass power.
  Relay rel_bypass_power_;
  /// Relay for bypass direction.
  Relay rel_bypass_direction_;
  /// Start of last change to compute hysteresis and motor runtime.
  unsigned long last_change_time_millis_ = 0;
  /// Current state of the flap.
  SummerBypassFlapState state_ = SummerBypassFlapState::UNKNOWN;
  /// Desired flap state.
  SummerBypassFlapState flap_setpoint_ = SummerBypassFlapState::UNKNOWN;
  /// State last communicated by MQTT.
  SummerBypassFlapState mqtt_state_ = SummerBypassFlapState::UNKNOWN;
  /// Set when motor is running and moving the flap.
  bool bypass_motor_running_ = false;
  /// Countdown for MQTT send.
  int8_t mqtt_countdown_ = 0;
  /// Task to publish MQTT values.
  PublishTask publish_task_;
  /// Task runtime statistics.
  Scheduler::TaskTimingStats stats_;
  /// Task scheduling bypass check.
  Scheduler::TimedTask<SummerBypass> timer_task_;
};
