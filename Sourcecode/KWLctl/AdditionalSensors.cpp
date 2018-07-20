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

#include "AdditionalSensors.h"
#include "KWLConfig.h"
#include "MessageHandler.h"
#include "MQTTTopic.hpp"

#include <DHT.h>
#include <DHT_U.h>

// Definitionen für das Scheduling

/// Interval between two DHT sensor readings (10s).
static constexpr unsigned long INTERVAL_DHT_READ              = 10000000;
/// Interval between two CO2 sensor readings (10s).
static constexpr unsigned long INTERVAL_MHZ14_READ            = 10000000;
/// Initial time before reading VOC sensor for the first time (30s).
static constexpr unsigned long INITIAL_INTERVAL_TGS2600_READ  = 30000000;
/// Time between VOC sensor readings (1s).
static constexpr unsigned long INTERVAL_TGS2600_READ          =  1000000;

/// Minimum time between communicating DHT values.
static constexpr unsigned long INTERVAL_MQTT_DHT              =  5000000;
/// Maximum time between communicating DHT values.
static constexpr unsigned long INTERVAL_MQTT_DHT_FORCE        = 300000000; // 5 * 60 * 1000; 5 Minuten
/// Minimum time between communicating CO2 values.
static constexpr unsigned long INTERVAL_MQTT_MHZ14            = 60000000;
/// Maximum time between communicating CO2 values.
static constexpr unsigned long INTERVAL_MQTT_MHZ14_FORCE      = 300000000; // 5 * 60 * 1000; 5 Minuten
/// Minimum time between communicating VOC values.
static constexpr unsigned long INTERVAL_MQTT_TGS2600          =  5000000;
/// Maximum time between communicating VOC values.
static constexpr unsigned long INTERVAL_MQTT_TGS2600_FORCE    = 30000000;

// DHT Sensoren
static DHT_Unified dht1(KWLConfig::PinDHTSensor1, DHT22);
static DHT_Unified dht2(KWLConfig::PinDHTSensor2, DHT22);

// TGS2600
static constexpr float  TGS2600_DEFAULTPPM        = 10;           //default ppm of CO2 for calibration
static constexpr long   TGS2600_DEFAULTRO         = 45000;        //default Ro for TGS2600_DEFAULTPPM ppm of CO2
static constexpr double TGS2600_SCALINGFACTOR     = 0.3555567714; //CO2 gas value
static constexpr double TGS2600_EXPONENT          = -3.337882361; //CO2 gas value
//static constexpr double TGS2600_MAXRSRO           = 2.428;        //for CO2
//static constexpr double TGS2600_MINRSRO           = 0.358;        //for CO2
static constexpr double TGS2600_RL                = 1000;

// **************************** CO2 Sensor MH-Z14 ******************************************
static const uint8_t cmdReadGasPpm[9]   = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
//static const uint8_t cmdCalZeroPoint[9] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78};
//static constexpr int Co2Min = 402;

//static char getChecksum(char *packet) {
//  char i, checksum;
//  checksum = 0;
//  for (i = 1; i < 8; i++) {
//    checksum += packet[i];
//  }
//  checksum = 0xff - checksum;
//  checksum += 1;
//  return checksum;
//}

// ----------------------------- TGS2600 ------------------------------------

// TODO there are many warnings when computing the value. For now, added
// explicit casts to proper types and/or cleaned up types, but it needs
// to be checked.

/*
   get the calibrated ro based upon read resistance, and a know ppm
*/
static double TGS2600_getro(double resvalue, float ppm) {
  return resvalue * exp(log(TGS2600_SCALINGFACTOR / double(ppm)) / TGS2600_EXPONENT);
}

/*
   get the ppm concentration
*/
static double TGS2600_getppm(double resvalue, long ro) {
  return TGS2600_SCALINGFACTOR * pow((resvalue / ro), TGS2600_EXPONENT);
}

static int calcSensor_VOC(int valr)
{
  double val;
  double TGS2600_ro = 0;  // unused except for debugging
  double val_voc = 0;

  //Serial.println(valr);
  if (valr > 0) {
    val =  (TGS2600_RL * (1024 - valr) / valr);
    TGS2600_ro = TGS2600_getro(val, TGS2600_DEFAULTPPM);
    //convert to ppm (using default ro)
    val_voc = TGS2600_getppm(val, TGS2600_DEFAULTRO);
  }
  if (KWLConfig::serialDebugSensor) {
    Serial.print ( F("Vrl / Rs / ratio:"));
    Serial.print ( val);
    Serial.print ( F(" / "));
    Serial.print ( TGS2600_ro);
    Serial.print ( F(" / "));
    Serial.println (val_voc);
  }
  return int(val_voc);
}

// ----------------------------- TGS2600 END --------------------------------


AdditionalSensors::AdditionalSensors() :
  stats_(F("AdditionalSensors")),
  dht1_read_(stats_, &AdditionalSensors::readDHT1, *this),
  dht2_read_(stats_, &AdditionalSensors::readDHT2, *this),
  mhz14_read_(stats_, &AdditionalSensors::readMHZ14, *this),
  voc_read_(stats_, &AdditionalSensors::readVOC, *this),
  dht_send_task_(stats_, &AdditionalSensors::sendDHT, *this, false),
  dht_send_oversample_task_(stats_, &AdditionalSensors::sendDHT, *this, true),
  co2_send_task_(stats_, &AdditionalSensors::sendCO2, *this, false),
  co2_send_oversample_task_(stats_, &AdditionalSensors::sendCO2, *this, true),
  voc_send_task_(stats_, &AdditionalSensors::sendVOC, *this, false),
  voc_send_oversample_task_(stats_, &AdditionalSensors::sendVOC, *this, true)
{}

bool AdditionalSensors::setupMHZ14()
{
  MHZ14_available_ = false;

  uint8_t response[9];

  KWLConfig::SerialMHZ14.begin(9600);
  delay(100);
  KWLConfig::SerialMHZ14.write(cmdReadGasPpm, 9);
  if (KWLConfig::SerialMHZ14.readBytes(response, 9) == 9) {
    int responseHigh = response[2];
    int responseLow = response[3];
    int ppm = (256 * responseHigh) + responseLow;
    if (KWLConfig::serialDebugSensor) {
      Serial.print(F("CO2 ppm: "));
      Serial.println(ppm);
    }
    if (ppm > 0) {
      mhz14_read_.runRepeated(INTERVAL_MHZ14_READ);
      MHZ14_available_ = true;
    }
  }
  return MHZ14_available_;
}

bool AdditionalSensors::setupTGS2600()
{
  pinMode(KWLConfig::PinVocSensor, INPUT_PULLUP);
  int analogVal = analogRead(KWLConfig::PinVocSensor);
  if (KWLConfig::serialDebugSensor) Serial.println(analogVal);
  if (analogVal < 1023) {
    voc_read_.runRepeated(INITIAL_INTERVAL_TGS2600_READ, INTERVAL_TGS2600_READ);
    TGS2600_available_ = true;
  } else {
    TGS2600_available_ = false;
  }
  return TGS2600_available_;
}

void AdditionalSensors::readDHT1()
{
  sensors_event_t event;

  dht1.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Failed reading temperature from DHT1"));
  }
  else if (event.temperature != dht1_temp_) {
    dht1_temp_ = event.temperature;
    Serial.print(F("DHT1 T: "));
    Serial.println(dht1_temp_);
  }

  dht1.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Failed reading humidity from DHT1"));
  }
  else if (event.relative_humidity != dht1_hum_) {
    dht1_hum_ = event.relative_humidity;
    Serial.print(F("DHT1 H: "));
    Serial.println(dht1_hum_);
  }
}

void AdditionalSensors::readDHT2()
{
  sensors_event_t event;

  dht2.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Failed reading temperature from DHT2"));
  }
  else if (event.temperature != dht2_temp_) {
    dht2_temp_ = event.temperature;
    Serial.print(F("DHT2 T: "));
    Serial.println(dht2_temp_);
  }

  dht2.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Failed reading humidity from DHT2"));
  }
  else if (event.relative_humidity != dht2_hum_) {
    dht2_hum_ = event.relative_humidity;
    Serial.print(F("DHT2 H: "));
    Serial.println(dht2_hum_);
  }
}

void AdditionalSensors::readMHZ14()
{
  uint8_t response[9];
  KWLConfig::SerialMHZ14.write(cmdReadGasPpm, 9);
  if (KWLConfig::SerialMHZ14.readBytes(response, 9) == 9) {
    int responseHigh = response[2];
    int responseLow = response[3];
    int ppm = (256 * responseHigh) + responseLow;

    if (KWLConfig::serialDebugSensor) {
      Serial.print(F("CO2 ppm: "));
      Serial.println(ppm);
    }
    // Automatische Kalibrieren des Nullpunktes auf den kleinstmöglichen Wert
    //if (ppm < Co2Min)
    //  KWLConfig::SerialMHZ14.write(cmdCalZeroPoint, 9);

    co2_ppm_ = ppm;
  }
}

void AdditionalSensors::readVOC()
{
  analogRead(KWLConfig::PinVocSensor);  // discard a read to get more stable reading
  int analogVal = analogRead(KWLConfig::PinVocSensor);
  if (KWLConfig::serialDebugSensor) {
    Serial.print(F("VOC analogVal: "));
    Serial.println(analogVal);
  }
  voc_ = calcSensor_VOC(analogVal);
}

void AdditionalSensors::begin(Print& initTracer)
{
  initTracer.print(F("Initialisierung Sensoren:"));
  // DHT Sensoren
  dht1.begin();
  dht2.begin();
  delay(1500);
  sensors_event_t event;
  dht1.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    DHT1_available_ = true;
    initTracer.print(F(" DHT1"));
    dht1_read_.runRepeated(INTERVAL_DHT_READ);
  }
  dht2.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    DHT2_available_ = true;
    initTracer.print(F(" DHT2"));
    dht2_read_.runRepeated(INTERVAL_DHT_READ);
  }
  if (DHT1_available_ || DHT2_available_) {
    dht_send_task_.runRepeated(INTERVAL_DHT_READ + 1000000, INTERVAL_MQTT_DHT);
    dht_send_oversample_task_.runRepeated(INTERVAL_DHT_READ + 1000000, INTERVAL_MQTT_DHT_FORCE);
  }

  // MH-Z14 CO2 Sensor
  if (setupMHZ14()) {
    initTracer.print(F(" CO2"));
    co2_send_task_.runRepeated(INTERVAL_MHZ14_READ + 1000000, INTERVAL_MQTT_MHZ14);
    co2_send_oversample_task_.runRepeated(INTERVAL_MHZ14_READ + 1000000, INTERVAL_MQTT_MHZ14_FORCE);
  }

  // TGS2600 VOC Sensor
  if (setupTGS2600()) {
    initTracer.print(F(" VOC"));
    voc_send_task_.runRepeated(INITIAL_INTERVAL_TGS2600_READ + 1000000, INTERVAL_MQTT_TGS2600);
    voc_send_oversample_task_.runRepeated(INITIAL_INTERVAL_TGS2600_READ + 1000000, INTERVAL_MQTT_TGS2600_FORCE);
  }

  if (!DHT1_available_ && !DHT2_available_ && !MHZ14_available_ && !TGS2600_available_) {
    initTracer.println(F(" keine Sensoren"));
  } else {
    initTracer.println();
  }
}

void AdditionalSensors::forceSend() noexcept
{
  if (DHT1_available_ || DHT2_available_)
    sendDHT(true);
  if (MHZ14_available_)
    sendCO2(true);
  if (TGS2600_available_)
    sendVOC(true);
}

void AdditionalSensors::sendDHT(bool force) noexcept
{
  if (!force
      && (abs(dht1_temp_ - dht1_last_sent_temp_) < 0.1f) && (abs(dht2_temp_ - dht2_last_sent_temp_) < 0.1f)
      && (abs(dht1_hum_ - dht2_last_sent_hum_) < 1) && (abs(dht2_hum_ - dht2_last_sent_hum_) < 1)) {
    // not enough change, no point to send
    return;
  }
  dht1_last_sent_temp_ = dht1_temp_;
  dht1_last_sent_hum_  = dht1_hum_;
  dht2_last_sent_temp_ = dht2_temp_;
  dht2_last_sent_hum_  = dht2_hum_;
  uint8_t bitmap = (DHT1_available_ ? 0x3 : 0) | (DHT2_available_ ? 0xc: 0);
  publish_dht_.publish([this, bitmap]() mutable {
    uint8_t bit = 1;
    while (bit < 16) {
      if (bitmap & bit) {
        bool res = true;
        switch (bit) {
          case 1: res = MessageHandler::publish(MQTTTopic::KwlDHT1Temperatur, dht1_temp_, 1, KWLConfig::RetainAdditionalSensors); break;
          case 2: res = MessageHandler::publish(MQTTTopic::KwlDHT1Humidity, dht1_hum_, 1, KWLConfig::RetainAdditionalSensors); break;
          case 4: res = MessageHandler::publish(MQTTTopic::KwlDHT2Temperatur, dht2_temp_, 1, KWLConfig::RetainAdditionalSensors); break;
          case 8: res = MessageHandler::publish(MQTTTopic::KwlDHT2Humidity, dht2_hum_, 1, KWLConfig::RetainAdditionalSensors); break;
          default: return true; // paranoia
        }
        if (!res)
          return false; // will retry later
        bitmap &= ~bit;
      }
      bit <<= 1;
    }
    return true;  // all sent
  });
  dht_send_task_.runRepeated(INTERVAL_MQTT_DHT);
  dht_send_oversample_task_.runRepeated(INTERVAL_MQTT_DHT_FORCE);
}

void AdditionalSensors::sendCO2(bool force) noexcept
{
  if (!force && (abs(co2_ppm_ - co2_last_sent_ppm_) < 20)) {
    // not enough change
    return;
  }
  publish_co2_.publish(MQTTTopic::KwlCO2Abluft, co2_ppm_, KWLConfig::RetainAdditionalSensors);
  co2_send_task_.runRepeated(INTERVAL_MQTT_MHZ14);
  co2_send_oversample_task_.runRepeated(INTERVAL_MQTT_MHZ14_FORCE);
}

void AdditionalSensors::sendVOC(bool force) noexcept
{
  // TODO this was not sent previously, why? Which threshold to use?
  if (!force && (abs(voc_ - voc_last_sent_) < 10)) {
    // not enough change
    return;
  }
  publish_voc_.publish(MQTTTopic::KwlVOCAbluft, voc_, 1, KWLConfig::RetainAdditionalSensors);
  voc_send_task_.runRepeated(INTERVAL_MQTT_TGS2600);
  voc_send_oversample_task_.runRepeated(INTERVAL_MQTT_TGS2600_FORCE);
}
