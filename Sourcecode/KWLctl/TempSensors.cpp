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

#include "TempSensors.h"
#include "MQTTTopic.hpp"

#include "kwl_config.h"

#include <PubSubClient.h>
#include <Arduino.h>

/// Precision of temperature reading (9-12 bits; 12 bits is 0.0625C, 9 bits is 0.5C).
static constexpr uint8_t TEMPERATURE_PRECISION = 12;
/// Scheduling interval for temperature sensor query (1s).
static constexpr unsigned long SCHEDULING_INTERVAL = 1000000;

TempSensors::TempSensor::TempSensor(uint8_t pin) :
  onewire_ifc_(pin),
  sensor_(&onewire_ifc_)
{
  sensor_.begin();
  sensor_.setResolution(TEMPERATURE_PRECISION);
  sensor_.setWaitForConversion(false);
  if (!sensor_.getAddress(address_, 0))
    state_ = -1;  // re-request
}

bool TempSensors::TempSensor::loop()
{
  if (state_ == 0) {
    // new reading requested
    sensor_.requestTemperatures();
    state_ = 1;
  } else if (state_ < 0) {
    // request address again upon retry
    if (sensor_.getAddress(address_, 0))
      state_ = 0; // successfull query
    // else state_ stays at -1 to retry getting the address next time
  } else if (state_ <= MAX_WAIT_TIME) {
    if (sensor_.isConversionComplete()) {
      // data can be read
      auto res = sensor_.getTempC(address_);
      if (res > DEVICE_DISCONNECTED_C) {
        // successful reading, start next read next time
        t_ = double(res);
        state_ = 0;
        return true;
      } else {
        // error reading data
        retry();
      }
    }
  } else {
    // retry read next time, it took too long
    retry();
  }
  return false;
}

void TempSensors::TempSensor::retry()
{
  if (retry_count_ >= MAX_RETRIES) {
    t_ = INVALID;
  } else {
    ++retry_count_;
  }
  state_ = -1; // start next retry
}

TempSensors::TempSensors(Scheduler& sched, Print& initTrace) :
  InitTrace(F("Initialisierung Temperatursensoren"), initTrace),
  Task("TempSensors"),
  t1_(kwl_config::PinTemp1OneWireBus),
  t2_(kwl_config::PinTemp2OneWireBus),
  t3_(kwl_config::PinTemp3OneWireBus),
  t4_(kwl_config::PinTemp4OneWireBus)
{
  // call every 1/8th of the second
  sched.addRepeated(*this, SCHEDULING_INTERVAL);
}

void TempSensors::run()
{
  // sensor reading handling
  TempSensor* t;
  switch (next_sensor_) {
    case 0: t = &t1_; next_sensor_ = 1; break;
    case 1: t = &t2_; next_sensor_ = 2; break;
    case 2: t = &t3_; next_sensor_ = 3; break;
    default: t = &t4_; next_sensor_ = 0; break;
  }
  auto new_temp = t->loop();
  if (new_temp || force_send_) {
    // compute efficiency
    auto diff_out = get_t3_outlet() - get_t1_outside();
    if (abs(diff_out) > 0.1) {
      auto diff_in = get_t2_inlet() - get_t1_outside();
      efficiency_ = int((100 * diff_in) / diff_out);
      efficiency_ = constrain(efficiency_, 0, 100);
    } else {
      efficiency_ = 0;
    }
  }

  sendMQTT();
}

bool TempSensors::mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int /*length*/)
{
  if (topic == MQTTTopic::CmdGetTemp) {
    forceSend();
  }
#ifdef DEBUG
  // TODO this should also disable updating temperatures via sensors
  else if (topic == MQTTTopic::KwlDebugsetTemperaturAussenluft) {
    get_t1_outside() = strtod(payload, nullptr);
    forceSend();
  }
  else if (topic == MQTTTopic::KwlDebugsetTemperaturZuluft) {
    get_t2_inlet() = strtod(payload, nullptr);
    forceSend();
  }
  else if (topic == MQTTTopic::KwlDebugsetTemperaturAbluft) {
    get_t3_outlet() = strtod(payload, nullptr);
    forceSend();
  }
  else if (topic == MQTTTopic::KwlDebugsetTemperaturFortluft) {
    get_t4_exhaust() = strtod(payload, nullptr);
    forceSend();
  }
#endif
  else {
    return false;
  }
  return true;
}

void TempSensors::sendMQTT() {
  // Send the temperatures via MQTT:
  //   - if forced, send,
  //   - if max time reached, send,
  //   - if min time reached and min difference found, send,
  //   - else wait for the next call.
  ++mqtt_ticks_;
  if (mqtt_ticks_ >= kwl_config::MaxIntervalMqttTemp || force_send_ ||
      (mqtt_ticks_ >= kwl_config::MinIntervalMqttTemp && (
         (abs(get_t1_outside() - last_mqtt_t1_) > kwl_config::MinDiffMqttTemp) ||
         (abs(get_t2_inlet() - last_mqtt_t2_) > kwl_config::MinDiffMqttTemp) ||
         (abs(get_t3_outlet() - last_mqtt_t3_) > kwl_config::MinDiffMqttTemp) ||
         (abs(get_t4_exhaust() - last_mqtt_t4_) > kwl_config::MinDiffMqttTemp)
      ))
     ) {
    last_mqtt_t1_ = get_t1_outside();
    last_mqtt_t2_ = get_t2_inlet();
    last_mqtt_t3_ = get_t3_outlet();
    last_mqtt_t4_ = get_t4_exhaust();

    auto r1 = publish(MQTTTopic::KwlTemperaturAussenluft, last_mqtt_t1_, 2, kwl_config::RetainTemperature);
    auto r2 = publish(MQTTTopic::KwlTemperaturZuluft, last_mqtt_t2_, 2, kwl_config::RetainTemperature);
    auto r3 = publish(MQTTTopic::KwlTemperaturAbluft, last_mqtt_t3_, 2, kwl_config::RetainTemperature);
    auto r4 = publish(MQTTTopic::KwlTemperaturFortluft, last_mqtt_t4_, 2, kwl_config::RetainTemperature);
    auto r5 = publish(MQTTTopic::KwlEffiency, getEfficiency(), kwl_config::RetainTemperature);

#if 0
    // TODO revive after respective stuff moved in their components
    if (antifreezeState) {
      mqtt_client_.publish(MQTTTopic::KwlAntifreeze, "on");
    } else {
      mqtt_client_.publish(MQTTTopic::KwlAntifreeze, "off");
    }
    if (heatingAppCombUse == 1) {
      mqtt_client_.publish(MQTTTopic::KwlHeatingAppCombUse, "YES");
    } else {
      mqtt_client_.publish(MQTTTopic::KwlHeatingAppCombUse, "NO");
    }
#endif

    // NOTE: in case we can't send something, force sending next time
    mqtt_ticks_ = 0;
    force_send_ = !(r1 && r2 && r3 && r4 && r5);
  }
}

