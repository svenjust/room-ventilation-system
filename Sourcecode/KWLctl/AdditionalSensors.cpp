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
unsigned long intervalDHTRead                = 10000;
unsigned long intervalMHZ14Read              = 10000;
unsigned long intervalTGS2600Read            = 30000;

unsigned long intervalMqttTemp               = 5000;
unsigned long intervalMqttMHZ14              = 60000;
unsigned long intervalMqttTempOversampling   = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttMHZ14Oversampling  = 300000; // 5 * 60 * 1000; 5 Minuten

unsigned long previousMillisDHTRead               = 0;
unsigned long previousMillisMHZ14Read             = 0;
unsigned long previousMillisTGS2600Read           = 0;

unsigned long previousMillisMqttDht               = 0;
unsigned long previousMillisMqttDhtOversampling   = 0;
unsigned long previousMillisMqttMHZ14             = 0;
unsigned long previousMillisMqttMHZ14Oversampling = 0;

float SendMqttDHT1Temp = 0;    // Temperatur Außenluft, gesendet per Mqtt
float SendMqttDHT1Hum  = 0;    // Temperatur Zuluft
float SendMqttDHT2Temp = 0;    // Temperatur Abluft
float SendMqttDHT2Hum  = 0;    // Temperatur Fortluft

int   SendMqttMHZ14CO2 = 0;    // CO2 Wert

// Sind die folgenden Variablen auf true, werden beim nächsten Durchlauf die entsprechenden mqtt Messages gesendet,
// anschliessend wird die Variable wieder auf false gesetzt
boolean mqttCmdSendDht                         = false;
boolean mqttCmdSendMHZ14                       = false;

// DHT Sensoren
DHT_Unified dht1(KWLConfig::PinDHTSensor1, DHT22);
DHT_Unified dht2(KWLConfig::PinDHTSensor2, DHT22);
boolean DHT1IsAvailable = false;
boolean DHT2IsAvailable = false;
float DHT1Temp = 0;
float DHT2Temp = 0;
float DHT1Hum  = 0;
float DHT2Hum  = 0;

// CO2 Sensor MH-Z14
boolean MHZ14IsAvailable = false;
int     MHZ14_CO2_ppm = -1;

// VOC Sensor TG2600
boolean TGS2600IsAvailable = false;
float   TGS2600_VOC   = -1.0;


// TGS2600
#define TGS2600_DEFAULTPPM         10             //default ppm of CO2 for calibration
long    TGS2600_DEFAULTRO        = 45000;         //default Ro for TGS2600_DEFAULTPPM ppm of CO2
#define TGS2600_SCALINGFACTOR      0.3555567714   //CO2 gas value
#define TGS2600_EXPONENT           -3.337882361   //CO2 gas value
#define TGS2600_MAXRSRO            2.428          //for CO2
#define TGS2600_MINRSRO            0.358          //for CO2
float   TGS2600_RL               = 1000;

int cycleMHZ14Counter = 0;
int tachoMHZ14TimeSum  = 0;
volatile byte tachoMHZ14CountSum       = 0;
volatile long tachoMHZ14LastMillis = 0;



void loopDHTRead() {
  // Diese Routine liest den oder die DHT Sensoren aus
  auto currentMillis = millis();
  if (currentMillis - previousMillisDHTRead >= intervalDHTRead) {
    previousMillisDHTRead = currentMillis;
    sensors_event_t event;

    if (DHT1IsAvailable) {
      dht1.temperature().getEvent(&event);
      if (isnan(event.temperature)) {
        Serial.println(F("Failed reading temperature from DHT1"));
      }
      else if (event.temperature != DHT1Temp) {
        DHT1Temp = event.temperature;
        Serial.print(F("DHT1 T: "));
        Serial.println(event.temperature);
      }

      dht1.humidity().getEvent(&event);
      if (isnan(event.relative_humidity)) {
        Serial.println(F("Failed reading humidity from DHT1"));
      }
      else if (event.relative_humidity != DHT1Hum) {
        DHT1Hum = event.relative_humidity;
        Serial.print(F("DHT1 H: "));
        Serial.println(event.relative_humidity);
      }
    }
    if (DHT2IsAvailable) {
      dht2.temperature().getEvent(&event);
      if (isnan(event.temperature)) {
        Serial.println(F("Failed reading temperature from DHT2"));
      }
      else if (event.temperature != DHT2Temp) {
        DHT2Temp = event.temperature;
        Serial.print(F("DHT2 T: "));
        Serial.println(event.temperature);
      }

      dht2.humidity().getEvent(&event);
      if (isnan(event.relative_humidity)) {
        Serial.println(F("Failed reading humidity from DHT2"));
      }
      else if (event.relative_humidity != DHT2Hum) {
        DHT2Hum = event.relative_humidity;
        Serial.print(F("DHT2 H: "));
        Serial.println(event.relative_humidity);
      }
    }
  }
}

// **************************** CO2 Sensor MH-Z14 ******************************************
const uint8_t cmdReadGasPpm[9]   = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
const uint8_t cmdCalZeroPoint[9] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78};
#define Co2Min 402

boolean SetupMHZ14() {
  MHZ14IsAvailable = false;

  int ppm = 0;
  uint8_t response[9];

  KWLConfig::SerialMHZ14.begin(9600);
  delay(100);
  KWLConfig::SerialMHZ14.write(cmdReadGasPpm, 9);
  if (KWLConfig::SerialMHZ14.readBytes(response, 9) == 9) {
    int responseHigh = (int) response[2];
    int responseLow = (int) response[3];
    int ppm = (256 * responseHigh) + responseLow;
    if (KWLConfig::serialDebugSensor) {
      Serial.print(F("CO2 ppm: "));
      Serial.println(ppm);
    }
    if (ppm > 0)
      MHZ14IsAvailable = true;
  }
  return MHZ14IsAvailable;
}


void loopMHZ14Read() {
  // Auslesen des CO2 Sensors
  if (MHZ14IsAvailable) {
    auto currentMillis = millis();
    if (currentMillis - previousMillisMHZ14Read >= intervalMHZ14Read) {
      previousMillisMHZ14Read = currentMillis;

      uint8_t response[9];
      KWLConfig::SerialMHZ14.write(cmdReadGasPpm, 9);
      if (KWLConfig::SerialMHZ14.readBytes(response, 9) == 9) {
        int responseHigh = (int) response[2];
        int responseLow = (int) response[3];
        int ppm = (256 * responseHigh) + responseLow;

        if (KWLConfig::serialDebugSensor) {
          Serial.print(F("CO2 ppm: "));
          Serial.println(ppm);
        }
        // Automatische Kalibrieren des Nullpunktes auf den kleinstmöglichen Wert
        //if (ppm < Co2Min) MHZ14CalibrateZeroPoint;

        MHZ14_CO2_ppm = ppm;
      }
    }
  }
}

void MHZ14CalibrateZeroPoint() {
  if (MHZ14IsAvailable) {
    KWLConfig::SerialMHZ14.write(cmdCalZeroPoint, 9);
  }
}

char getChecksum(char *packet) {
  char i, checksum;
  checksum = 0;
  for (i = 1; i < 8; i++) {
    checksum += packet[i];
  }
  checksum = 0xff - checksum;
  checksum += 1;
  return checksum;
}


// ----------------------------- TGS2600 ------------------------------------

/*
   get the calibrated ro based upon read resistance, and a know ppm
*/
long TGS2600_getro(float resvalue, float ppm) {
  return (long)(resvalue * exp( log(TGS2600_SCALINGFACTOR / ppm) / TGS2600_EXPONENT ));
}

/*
   get the ppm concentration
*/
float TGS2600_getppm(long resvalue, long ro) {
  float ret = 0;
  ret = (float)TGS2600_SCALINGFACTOR * pow(((float)resvalue / ro), TGS2600_EXPONENT);
  return ret;
}

float calcSensor_VOC(int valr)
{
  float val;
  float TGS2600_ro;
  float val_voc;

  //Serial.println(valr);
  if (valr > 0) {
    val =  ((float)TGS2600_RL * (1024 - valr) / valr);
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
  return val_voc;
}


void loopVocRead() {
  if (TGS2600IsAvailable) {
    auto currentMillis = millis();
    intervalTGS2600Read = 1000;
    if (currentMillis - previousMillisTGS2600Read >= intervalTGS2600Read) {
      previousMillisTGS2600Read = currentMillis;
      int analogVal = analogRead(KWLConfig::PinVocSensor);
      if (KWLConfig::serialDebugSensor) {
        Serial.print(F("VOC analogVal: "));
        Serial.println(analogVal);
      }
      TGS2600_VOC = calcSensor_VOC(analogVal);
    }
  }
}

boolean SetupTGS2600() {
  pinMode(KWLConfig::PinVocSensor, INPUT_PULLUP);
  int analogVal = analogRead(KWLConfig::PinVocSensor);
  if (KWLConfig::serialDebugSensor) Serial.println(analogVal);
  if (analogVal < 1023) {
    TGS2600IsAvailable = true;
  } else {
    TGS2600IsAvailable = false;
  }
  return TGS2600IsAvailable;
}

// loopMqtt... senden Werte an den mqtt-Server.

void loopMqttSendDHT() {
  // Senden der DHT Temperaturen und Humidity per Mqtt
  // Bedingung: a) alle x Sekunden, wenn Differenz Temperatur > 0.5
  //            b) alle intervalMqttTempOversampling/1000 Sekunden (Standard 5 Minuten)
  //            c) mqttCmdSendDht == true
  auto currentMillis = millis();
  if ((currentMillis - previousMillisMqttDht > intervalMqttTemp) || mqttCmdSendDht) {
    previousMillisMqttDht = currentMillis;
    if ((abs(DHT1Temp - SendMqttDHT1Temp) > 0.5)
        || (abs(DHT2Temp - SendMqttDHT2Temp) > 0.5)
        || (abs(DHT1Hum - SendMqttDHT1Hum) > 1)
        || (abs(DHT2Hum - SendMqttDHT2Hum) > 1)
        || (currentMillis - previousMillisMqttDhtOversampling > intervalMqttTempOversampling)
        || mqttCmdSendDht)  {

      mqttCmdSendDht = false;
      SendMqttDHT1Temp = DHT1Temp;
      SendMqttDHT2Temp = DHT2Temp;
      SendMqttDHT1Hum = DHT1Hum;
      SendMqttDHT2Hum = DHT2Hum;
      previousMillisMqttDhtOversampling = currentMillis;

      if (DHT1IsAvailable) {
        MessageHandler::publish(MQTTTopic::KwlDHT1Temperatur, DHT1Temp, 2, KWLConfig::RetainAdditionalSensors);
        MessageHandler::publish(MQTTTopic::KwlDHT1Humidity, DHT1Hum, 2, KWLConfig::RetainAdditionalSensors);
      }
      if (DHT2IsAvailable) {
        MessageHandler::publish(MQTTTopic::KwlDHT2Temperatur, DHT2Temp, 2, KWLConfig::RetainAdditionalSensors);
        MessageHandler::publish(MQTTTopic::KwlDHT2Humidity, DHT2Hum, 2, KWLConfig::RetainAdditionalSensors);
      }
    }
  }
}

void loopMqttSendCo2() {
  // Senden der DHT Temperaturen und Humidity per Mqtt
  // Bedingung: a) alle x Sekunden, wenn Differenz Sensor > 20
  //            b) alle intervalMqttMHZ14Oversampling/1000 Sekunden (Standard 10 Minuten)
  //            c) mqttCmdSendMHZ14 == true
  auto currentMillis = millis();
  if ((currentMillis - previousMillisMqttMHZ14 > intervalMqttMHZ14) || mqttCmdSendMHZ14) {
    previousMillisMqttMHZ14 = currentMillis;
    if ((abs(MHZ14_CO2_ppm - SendMqttMHZ14CO2) > 20)
        || (currentMillis - previousMillisMqttMHZ14Oversampling > intervalMqttMHZ14Oversampling)
        || mqttCmdSendMHZ14)  {

      mqttCmdSendMHZ14 = false;
      SendMqttMHZ14CO2 = MHZ14_CO2_ppm;

      intervalMqttMHZ14Oversampling = currentMillis;

      if (MHZ14IsAvailable) {
        MessageHandler::publish(MQTTTopic::KwlCO2Abluft, MHZ14_CO2_ppm, KWLConfig::RetainAdditionalSensors);
      }
    }
  }
}

AdditionalSensors::AdditionalSensors() :
  Task(F("AdditionalSensors"), *this, &AdditionalSensors::run)
{
  setInterval(1000000);  // run every second, each sensor times itself separately
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
    DHT1IsAvailable = true;
    initTracer.print(F(" DHT1"));
  }
  dht2.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    DHT2IsAvailable = true;
    initTracer.print(F(" DHT2"));
  }

  // MH-Z14 CO2 Sensor
  if (SetupMHZ14()) {
    initTracer.print(F(" CO2"));
  }

  // TGS2600 VOC Sensor
  if (SetupTGS2600()) {
    initTracer.print(F(" VOC"));
  }
  if (!DHT1IsAvailable && !DHT2IsAvailable && !MHZ14IsAvailable && !TGS2600IsAvailable) {
    initTracer.println(F(" keine Sensoren"));
  } else {
    initTracer.println();
  }
}

void AdditionalSensors::forceSend()
{
  mqttCmdSendDht             = true;
  mqttCmdSendMHZ14           = true;
}

void AdditionalSensors::run()
{
  loopDHTRead();
  loopMHZ14Read();
  loopVocRead();
  loopMqttSendDHT();
  loopMqttSendCo2();
  // TODO why was VOC not sent at all?
}
