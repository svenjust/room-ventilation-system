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

#include "TimeScheduler.h"
#include "MessageHandler.h"

#include <OneWire.h>            // OneWire Temperatursensoren
#include <DallasTemperature.h>  // https://www.milesburton.com/Dallas_Temperature_Control_Library

/*!
 * @brief Collection of temperature sensors of the ventilation system.
 *
 * Sensors array will update in a loop scheduled by task scheduler.
 */
class TempSensors : private MessageHandler
{
  /// One temperature sensor
  class TempSensor
  {
  public:
    /*!
     * @brief Construct temperature sensor reading via given pin.
     *
     * @param pin pin on which to read the temperature.
     */
    explicit TempSensor(uint8_t pin);

    /// Initialize the sensor.
    void begin();

    /// Execute one loop, returns true if temperature read.
    bool loop();

    /// Get measured temperature or INVALID.
    inline double get_t() const { return t_; }

    /// Get measured temperature or INVALID.
    inline double& get_t() { return t_; }

  private:
    /// Maximum wait time to wait for sensor to respond before reporting INVALID and retrying (5s).
    static constexpr uint8_t MAX_WAIT_TIME = 3;
    /// After how many errors do we consider temperature sensor to be dead (~1 min).
    static constexpr uint8_t MAX_RETRIES = 5;

    void retry();

    OneWire onewire_ifc_;       ///< Interface on which to read.
    DallasTemperature sensor_;  ///< Sensor.
    DeviceAddress address_;     ///< Sensor address.
    int8_t state_ = 0;          ///< Current query state.
    uint8_t retry_count_ = 0;   ///< Retry count.
    double t_ = INVALID;        ///< Current temperature.
  };

public:
  /// Construct sensor array.
  TempSensors();

  /// Start sensors.
  void begin(Print& initTrace);

  /// Constant for invalid temperature.
  static constexpr double INVALID = -127.0;

  /// Get the temperature of outside air being pulled into the device (or INVALID if not available).
  inline double get_t1_outside() const { return t1_.get_t(); }
  /// Get the temperature of inlet air being pushed into the house (or INVALID if not available).
  inline double get_t2_inlet() const { return t2_.get_t(); }
  /// Get the temperature of outlet air being pulled from the house (or INVALID if not available).
  inline double get_t3_outlet() const { return t3_.get_t(); }
  /// Get the temperature of exhaust air being pushed to outside (or INVALID if not available).
  inline double get_t4_exhaust() const { return t4_.get_t(); }

  /// Get the temperature of outside air being pulled into the device (or INVALID if not available).
  inline double& get_t1_outside() { return t1_.get_t(); }
  /// Get the temperature of inlet air being pushed into the house (or INVALID if not available).
  inline double& get_t2_inlet() { return t2_.get_t(); }
  /// Get the temperature of outlet air being pulled from the house (or INVALID if not available).
  inline double& get_t3_outlet() { return t3_.get_t(); }
  /// Get the temperature of exhaust air being pushed to outside (or INVALID if not available).
  inline double& get_t4_exhaust() { return t4_.get_t(); }

  /// Get efficiency of the heat exchange in %.
  inline int getEfficiency() const { return efficiency_; }

  /// Force sending temperature messages via MQTT independent of timing.
  inline void forceSend() { sendMQTT(); }

private:
  void run();
  virtual bool mqttReceiveMsg(const StringView& topic, const StringView& s) override;

  /// Send messages via MQTT.
  void sendMQTT();

  TempSensor t1_; ///< Outside/intake temperature.
  TempSensor t2_; ///< Temperature of inlet air being pushed into the house.
  TempSensor t3_; ///< (Inside) temperature of outlet air being pulled from the house.
  TempSensor t4_; ///< Temperature of exhaust air being pushed to the outside.
  int efficiency_ = 0;        ///< Current efficiency of heat exchange.
  uint8_t next_sensor_ = 0;   ///< Next sensor to talk to.
  uint8_t mqtt_ticks_ = 0;    ///< MQTT seconds ticks.
  double last_mqtt_t1_ = INVALID; ///< Last T1 temperature sent via MQTT.
  double last_mqtt_t2_ = INVALID; ///< Last T2 temperature sent via MQTT.
  double last_mqtt_t3_ = INVALID; ///< Last T3 temperature sent via MQTT.
  double last_mqtt_t4_ = INVALID; ///< Last T4 temperature sent via MQTT.
  PublishTask publish_task_;      ///< Task to publish measurements.
  Scheduler::TaskTimingStats stats_;              ///< Task runtime statistics.
  Scheduler::TimedTask<TempSensors> timer_task_;  ///< Task for reading sensors periodically.
};
