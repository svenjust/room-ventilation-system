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
 * @brief Additional sensors of the ventilation system (optional).
 */
#pragma once

#include "TimeScheduler.h"
#include "MessageHandler.h"

class Print;

/*!
 * @brief Additional sensors of the ventilation system (optional).
 *
 * This class reads and publishes values of additional sensors like
 * humidity, CO2 and VOC.
 */
class AdditionalSensors
{
public:
  AdditionalSensors();

  /// Initialize sensors.
  void begin(Print& initTracer);

  /// Force sending values via MQTT on the next MQTT run.
  void forceSend() noexcept;

  /// Check if DHT1 sensor is present.
  bool hasDHT1() const noexcept { return DHT1_available_; }

  /// Get DHT1 temperature.
  float getDHT1Temp() const noexcept { return dht1_temp_; }

  /// Get DHT1 humidity.
  float getDHT1Hum() const noexcept { return dht1_hum_; }

  /// Check if DHT2 sensor is present.
  bool hasDHT2() const noexcept { return DHT2_available_; }

  /// Get DHT2 temperature.
  float getDHT2Temp() const noexcept { return dht2_temp_; }

  /// Get DHT2 humidity.
  float getDHT2Hum() const noexcept { return dht2_hum_; }

  /// Check if VOC sensor is present.
  bool hasVOC() const noexcept { return TGS2600_available_; }

  /// Get VOC sensor value.
  int getVOC() const noexcept { return voc_; }

  /// Check if CO2 sensor is present.
  bool hasCO2() const noexcept { return MHZ14_available_; }

  /// Get CO2 sensor value.
  int getCO2() const noexcept { return co2_ppm_; }

private:
  /// Set up CO2 sensor.
  bool setupMHZ14();
  /// Set up VOC sensor.
  bool setupTGS2600();

  /// Read value of DHT1.
  void readDHT1();
  /// Read value of DHT2.
  void readDHT2();
  /// Read value of CO2 sensor.
  void readMHZ14();
  /// Read value of air quality sensor.
  void readVOC();

  /// Schedule sending DHT values now.
  void sendDHT(bool force) noexcept;
  /// Schedule sending CO2 values now.
  void sendCO2(bool force) noexcept;
  /// Schedule sending CO2 values now.
  void sendVOC(bool force) noexcept;

  // sensor availability
  bool DHT1_available_ = false;
  bool DHT2_available_ = false;
  bool TGS2600_available_ = false;
  bool MHZ14_available_ = false;

  // current values
  float dht1_temp_ = 0;
  float dht2_temp_ = 0;
  float dht1_hum_ = 0;
  float dht2_hum_ = 0;
  int co2_ppm_ = -1;
  int voc_ = -1;

  // last sent values per MQTT
  float dht1_last_sent_temp_ = 0;
  float dht2_last_sent_temp_ = 0;
  float dht1_last_sent_hum_ = 0;
  float dht2_last_sent_hum_ = 0;
  int co2_last_sent_ppm_ = -1;
  int voc_last_sent_ = -1;

  // Tasks running on timeout
  Scheduler::TaskTimingStats stats_;
  Scheduler::TimedTask<AdditionalSensors> dht1_read_;
  Scheduler::TimedTask<AdditionalSensors> dht2_read_;
  Scheduler::TimedTask<AdditionalSensors> mhz14_read_;
  Scheduler::TimedTask<AdditionalSensors> voc_read_;

  Scheduler::TimedTask<AdditionalSensors, bool> dht_send_task_;
  Scheduler::TimedTask<AdditionalSensors, bool> dht_send_oversample_task_;
  Scheduler::TimedTask<AdditionalSensors, bool> co2_send_task_;
  Scheduler::TimedTask<AdditionalSensors, bool> co2_send_oversample_task_;
  Scheduler::TimedTask<AdditionalSensors, bool> voc_send_task_;
  Scheduler::TimedTask<AdditionalSensors, bool> voc_send_oversample_task_;

  // Tasks publishing MQTT values
  PublishTask publish_dht_;
  PublishTask publish_co2_;
  PublishTask publish_voc_;
};
