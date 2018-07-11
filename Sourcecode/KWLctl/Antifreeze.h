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
 * @brief Protection against freezing the heat exchange.
 */
#pragma once

#include "Task.h"
#include "MessageHandler.h"

#include <PID_v1.h>

class KWLPersistentConfig;
class FanControl;
class TempSensors;
class Print;

/// State of antifreeze control.
enum class AntifreezeState : uint8_t
{
  OFF       = 0,
  PREHEATER = 1,
  FAN_OFF   = 2,
  FIREPLACE = 3
};

/*!
 * @brief Protection against freezing the heat exchange.
 */
class Antifreeze : private Task, private MessageHandler
{
public:
  Antifreeze(const Antifreeze&) = delete;
  Antifreeze& operator=(const Antifreeze&) = delete;

  /*!
   * @brief Construct antifreeze control object.
   *
   * @param fan fan control.
   * @param temp temperature sensor array.
   * @param config configuration to read/write.
   */
  explicit Antifreeze(FanControl& fan, TempSensors& temp, KWLPersistentConfig& config);

  /// Start the handler.
  void begin(Print& initTracer);

  /// Get current state of antifreeze settings.
  AntifreezeState getState() const { return antifreeze_state_; }

  /// Get preheater settings (in %).
  double getPreheaterState() const { return tech_setpoint_preheater_ / 10; }

  /// Callback for fan control to set fan speed to 0, if needed.
  void doActionAntiFreezeState();

  /// Check whether using the ventilation system combined with heating application.
  bool getHeatingAppCombUse() const { return heating_app_comb_use_; }

  /// Set whether using the ventilation system combined with heating application.
  void setHeatingAppCombUse(bool on);

  /// Send MQTT messages on the next loop.
  void forceSend();

private:
  void run();
  virtual bool mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length) override;

  /// Set preheater output signal.
  void setPreheater();

  /// Send messages via MQTT.
  void sendMQTT();

  FanControl& fan_;
  TempSensors& temp_;
  KWLPersistentConfig& config_;
  AntifreezeState antifreeze_state_ = AntifreezeState::OFF;
  unsigned hysteresis_temp_delta_;
  double antifreeze_temp_upper_limit_;
  double tech_setpoint_preheater_   = 0.0;      // Analogsignal 0..1000 für Vorheizer
  unsigned long preheater_start_time_ms_ = 0;      // Beginn der Vorheizung
  unsigned long heating_app_comb_use_antifreeze_start_time_ms_ = 0;
  PID pid_preheater_;
  bool heating_app_comb_use_; ///< Flag whether we are using the ventilation system combined with heating appliance.
  bool force_send_ = true;    ///< Force sending MQTT messages (initially true to send initial state).
};

