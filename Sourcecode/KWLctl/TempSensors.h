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
 * @brief Collection of temperature sensors of the ventilation system.
 */
#pragma once

#include "Scheduler.h"
#include "InitTrace.h"
#include "MQTTClient.h"

#include <OneWire.h>            // OneWire Temperatursensoren
#include <DallasTemperature.h>  // https://www.milesburton.com/Dallas_Temperature_Control_Library

/*!
 * @brief Collection of temperature sensors of the ventilation system.
 *
 * Sensors array will update in a loop scheduled by task scheduler.
 */
class TempSensors : private InitTrace, private Task, private MessageHandler
{
  /// One temperature sensor
  class TempSensor
  {
  public:
    explicit TempSensor(uint8_t pin);

    /// Execute one loop. Returns true, if temperature changed.
    bool loop();

    /// Get measured temperature or INVALID.
    inline double& get_t() { return t_; }

  private:
    /// Maximum wait time to wait for sensor to respond before reporting INVALID and retrying.
    static constexpr uint8_t MAX_WAIT_TIME = 50;  // 5 seconds
    /// After how many errors do we consider temperature sensor to be dead (~1 min).
    static constexpr uint8_t MAX_RETRIES = 10;

    void retry();

    OneWire onewire_ifc_;       ///< Interface on which to read.
    DallasTemperature sensor_;  ///< Sensor.
    uint8_t state_ = 0;         ///< Current query state.
    uint8_t retry_count_ = 0;   ///< Retry count.
    double t_ = INVALID;        ///< Current temperature.
  };

public:
  /// Construct sensor read loop on the given scheduler and send via MQTT client.
  explicit TempSensors(Scheduler& sched, Print& initTrace);

  /// Constant for invalid temperature.
  static constexpr double INVALID = -127.0;

  /// Get the temperature of outside air being pulled into the device (or INVALID if not available).
  inline double& get_t1_outside() { return t1_.get_t(); }
  /// Get the temperature of inlet air being pushed into the house (or INVALID if not available).
  inline double& get_t2_inlet() { return t2_.get_t(); }
  /// Get the temperature of outlet air being pulled from the house (or INVALID if not available).
  inline double& get_t3_outlet() { return t3_.get_t(); }
  /// Get the temperature of exhaust air being pushed to outside (or INVALID if not available).
  inline double& get_t4_exhaust() { return t4_.get_t(); }

  /// Get efficiency of the heat exchange in %.
  inline int getEfficiency() { return efficiency_; }

  /// Force sending temperature messages via MQTT independent of timing.
  inline void forceSend() { force_send_ = true; }

private:
  virtual void run() override;
  virtual bool mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length) override;

  /// Send messages via MQTT.
  void sendMQTT();

  TempSensor t1_; ///< Outside/intake temperature.
  TempSensor t2_; ///< Temperature of inlet air being pushed into the house.
  TempSensor t3_; ///< (Inside) temperature of outlet air being pulled from the house.
  TempSensor t4_; ///< Temperature of exhaust air being pushed to the outside.
  int efficiency_ = 0;        ///< Current efficiency of heat exchange.
  bool force_send_ = false;   ///< Force sending temperatures via MQTT.
  uint8_t mqtt_countdown_;    ///< Countdown when to send MQTT message.
  uint8_t mqtt_ticks_ = 0;    ///< MQTT seconds ticks.
  double last_mqtt_t1_ = INVALID;
  double last_mqtt_t2_ = INVALID;
  double last_mqtt_t3_ = INVALID;
  double last_mqtt_t4_ = INVALID;
};
