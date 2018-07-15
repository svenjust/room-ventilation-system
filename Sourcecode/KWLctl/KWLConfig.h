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
 * @brief Configuration of the project.
 *
 * @note DO NOT MODIFY THIS FILE. Put your configuration into UserConfig.h.
 *
 * If no UserConfig.h is provided, then defaults are used. If it is provided,
 * then defaults may be overridden there, by using lines in the form:
 *
 * CONFIGURE(name, value)
 * CONFIGURE_OBJ(name, (values...))
 *
 * for each configuration parameter from KWLConfig.
 *
 * File UserConfig.h is not tracked, so it's easy to define specific site
 * configuration without modifying the tracked source code. Most likely, you
 * will at least want to override network settings.
 *
 * @par Example:
 * @code
 * CONFIGURE(RelayOFF, OPEN)
 * CONFIGURE_OBJ(NetworkIPAddress, (192, 168, 42, 201))
 * CONFIGURE_OBJ(NetworkMACAddress, (0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF))
 * @endcode
 *
 * You can also define DEBUG macro in UserConfig.h to enable debugging MQTT messages.
 */

#pragma once

#include <Arduino.h>

class IPAddress;
class HardwareSerial;

/// Helper to construct IP address as a literal.
class IPAddressLiteral
{
public:
  constexpr IPAddressLiteral(byte a, byte b, byte c, byte d) : ip{a, b, c, d} {}
  operator IPAddress() const;
  constexpr IPAddressLiteral operator&(const IPAddressLiteral& other) const {
    return IPAddressLiteral(ip[0] & other.ip[0], ip[1] & other.ip[1], ip[2] & other.ip[2], ip[3] & other.ip[3]);
  }
  constexpr IPAddressLiteral operator|(const IPAddressLiteral& other) const {
    return IPAddressLiteral(ip[0] | other.ip[0], ip[1] | other.ip[1], ip[2] | other.ip[2], ip[3] | other.ip[3]);
  }
private:
  byte ip[4];
};

/// Helper to construct MAC address as a literal.
class MACAddressLiteral
{
public:
  constexpr MACAddressLiteral(byte a, byte b, byte c, byte d, byte e, byte f) : mac{a, b, c, d, e,f} {}
  void copy_to(byte* out) const { out[0] = mac[0]; out[1] = mac[1]; out[2] = mac[2]; out[3] = mac[3]; out[4] = mac[4]; out[5] = mac[5]; }
private:
  byte mac[6];
};

/// Maximum # of fan mode settings. Not configurable.
static constexpr unsigned MAX_FAN_MODE_CNT = 10;

/// Do not build debug mode. Define in UserConfig.h if desired.
#undef DEBUG

/// Default configuration, which can be overridden by the user configuration below.
template<typename FinalConfig>
class KWLDefaultConfig
{
protected:
  /// Helper for relay state.
  static constexpr int8_t OPEN = -1;

public:

  // ***************************************************  N E T Z W E R K E I N S T E L L U N G E N *****************************************************
  // Hier die IP Adresse für diese Steuerung und den MQTT Broker definieren.

  /// MAC Adresse des Ethernet Shields.
  static constexpr MACAddressLiteral NetworkMACAddress = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };

  /// IP Adresse für diese Gerät im eigenen Netz.
  static constexpr IPAddressLiteral NetworkIPAddress = {192, 168,  20, 201};

  /// Subnet Mask.
  static constexpr IPAddressLiteral NetworkSubnetMask = {255, 255, 255,   0};

  /// Gateway, defaults to network address with "1" as last element.
  static const IPAddressLiteral NetworkGateway;

  /// DNS Server, defaults to gateway.
  static const IPAddressLiteral NetworkDNSServer;

  /// NTP (network time protocol) server, defaults to gateway.
  static const IPAddressLiteral NetworkNTPServer;

  /// IP Adresse des MQTT Brokers.
  static constexpr IPAddressLiteral NetworkMQTTBroker = {192, 168,  20, 240};

  /// Port des MQTT Brokers.
  static constexpr uint16_t NetworkMQTTPort = 1883;

  /// Login name für den MQTT Broker.
  static constexpr const char* NetworkMQTTUsername = nullptr;

  /// Passwort für den MQTT Broker.
  static constexpr const char* NetworkMQTTPassword = nullptr;

  // *******************************************E N D E ***  N E T Z W E R K E I N S T E L L U N G E N **************************************************


  // ******************************************* W E R K S E I N S T E L L U N G E N ********************************************************************
  // Definition der Werkeinstellungen.
  // Es können bis zu 10 Lüftungsstufen definiert werden. Im Allgemeinen sollten 4 oder 6 Stufen ausreichen.
  // Die Originalsteuerung stellt 3 Stufen zur Verfügung
  //
  // Eine Definition für 4 Stufen, ähnlich der Originalsteuerung wäre:
  // Stufe 0 = 0%, Stufe 1 = 70%, Stufe 2 = 100%, Stufe 3 = 130%. Stufe 0 ist hier zusätzlich.
  //
  // Ein mögliche Definition für 6 Stufen wäre bspw.:
  // Stufe 0 = 0%, Stufe 1 = 60%, Stufe 2 = 80%, Stufe 3 = 100%, Stufe 4 = 120%, Stufe 5 = 140%

  /// # der konfigurierten Standardlüftungsstufen.
  static constexpr unsigned StandardModeCnt = 4;
  /// Solldrehzahlen in Relation zur Standardlüftungsstufe.
  static constexpr double StandardKwlModeFactor[MAX_FAN_MODE_CNT] = {0, 0.7, 1, 1.3};
  /// Standardlüftungsstufe beim Anschalten.
  static constexpr int StandardKwlMode = 2;
  /// Drehzahl für Standardlüftungsstufe Zuluft.
  static constexpr unsigned StandardSpeedSetpointFan1       = 1550;              // sju: 1450
  /// Drehzahl für Standardlüftungsstufe Abluft.
  static constexpr unsigned StandardSpeedSetpointFan2       = 1550;              // sju: 1100
  /// Nenndrehzahl Papst Lüfter lt Datenblatt 3200 U/min.
  static constexpr unsigned StandardNenndrehzahlFan         = 3200;
  /// Mindestablufttemperatur für die Öffnung des Bypasses im Automatik Betrieb.
  static constexpr unsigned StandardBypassTempAbluftMin     =   24;
  /// Mindestaussenlufttemperatur für die Öffnung des Bypasses im Automatik Betrieb.
  static constexpr unsigned StandardBypassTempAussenluftMin =   13;
  /// Hystereszeit für eine Umstellung des Bypasses im Automatik Betrieb.
  static constexpr unsigned StandardBypassHystereseMinutes  =   60;
  /// Hysteretemperatur für eine Umstellung des Bypasses im Automatik Betrieb.
  static constexpr unsigned StandardBypassHysteresisTemp    =    2;
  /// Stellung der Bypassklappen im manuellen Betrieb (1 = Close).
  static constexpr unsigned StandardBypassManualSetpoint    =    1;
  /// Automatik oder manueller Betrieb der Bypassklappe (0 = Auto).
  // Im Automatikbetrieb steuert diese Steuerung die Bypass-Klappe, im manuellen Betrieb wird die Bypass-Klappe durch mqtt-Kommandos gesteuert.
  static constexpr unsigned StandardBypassMode              =    0;
  /// Hysteretemperatur für Steuerung von Antifreeze.
  static constexpr unsigned StandardAntifreezeHystereseTemp =    3;
  /// 0 = NO, 1 = YES
  static constexpr unsigned StandardHeatingAppCombUse       =    0;

  /// Daylight savings time default settings.
  static constexpr bool StandardDST                         = false;
  /// Standard time zone offset in minutes (CET, GMT+1).
  static constexpr int16_t StandardTimezoneMin              = 60;

  /*!
   * @brief Perform "factory reset" *at each startup*.
   *
   * If set, perform load of defaults on each startup. If not set, load EEPROM data.
   * DO NOT set this in productive environment, only as last resort to reset EEPROM.
   *
   * Factory reset can be also done using MQTT:
   * @code
   * mosquitto_pub -t d15/set/kwl/resetAll_IKNOWWHATIMDOING -m YES
   * @endcode
   */
  static constexpr bool FACTORY_RESET_EEPROM = false;

  // **************************************E N D E *** W E R K S E I N S T E L L U N G E N **************************************************************


  // ***************************************************  A N S C H L U S S E I N S T E L L U N G E N ***************************************************
  // Die Anschlüsse können dem Schaltplan entnommen werden: https://github.com/svenjust/room-ventilation-system/blob/master/Docs/circuit%20diagram/KWL-Steuerung%20f%C3%BCr%20P300.pdf

  /// Bypass Strom an/aus. Das BypassPower steuert, ob Strom am Bypass geschaltet ist, BypassDirection bestimmt Öffnen oder Schliessen.
  static constexpr uint8_t PinBypassPower     = 40;
  /// Bypass Richtung, Stromlos = Schliessen (Winterstellung), Strom = Öffnen (Sommerstellung).
  static constexpr uint8_t PinBypassDirection = 41;
  /// Stromversorgung Lüfter 1.
  static constexpr uint8_t PinFan1Power       = 42;
  /// Stromversorgung Lüfter 2.
  static constexpr uint8_t PinFan2Power       = 43;

  /// Steuerung Lüfter Zuluft per PWM Signal.
  static constexpr uint8_t PinFan1PWM         = 44;
  /// Steuerung Lüfter Abluft per PWM Signal.
  static constexpr uint8_t PinFan2PWM         = 46;
  /// Steuerung Vorheizregister per PWM Signal.
  static constexpr uint8_t PinPreheaterPWM    = 45;
  /// Eingang Lüfter Zuluft Tachosignal mit Interrupt, Zuordnung von Pin zu Interrupt geschieht im Code mit der Funktion digitalPinToInterrupt.
  static constexpr uint8_t PinFan1Tacho       = 18;
  /// Eingang Lüfter Abluft Tachosignal mit Interrupt, Zuordnung von Pin zu Interrupt geschieht im Code mit der Funktion digitalPinToInterrupt.
  static constexpr uint8_t PinFan2Tacho       = 19;
  /// Sampling für Tachoimpulse beim FALLING oder RISING.
  static constexpr int8_t TachoSamplingMode   = FALLING;

  // Alternative zu PWM, Ansteuerung per DAC. I2C nutzt beim Arduino Mega Pin 20 u 21.
  /// I2C-OUTPUT-Addresse für Horter DAC als 7 Bit, wird verwendet als Alternative zur PWM Ansteuerung der Lüfter und für Vorheizregister.
  static constexpr uint8_t DacI2COutAddr       = 176 >> 1;
  /// Kanal 1 des DAC für Zuluft.
  static constexpr uint8_t DacChannelFan1      = 0;
  /// Kanal 2 des DAC für Abluft.
  static constexpr uint8_t DacChannelFan2      = 1;
  /// Kanal 3 des DAC für Vorheizregister.
  static constexpr uint8_t DacChannelPreheater = 2;
  /// Zusätzliche Ansteuerung durch DAC über SDA und SLC (und PWM)
  static constexpr bool ControlFansDAC = true;

  /// Pin vom 1. DHT Sensor.
  static constexpr uint8_t PinDHTSensor1       = 28;
  /// Pin vom 2. DHT Sensor.
  static constexpr uint8_t PinDHTSensor2       = 29;

  // Für jeder Temperatursensor gibt es einen Anschluss auf dem Board, Vorteil: Temperatursensoren können per Kabel definiert werden, nicht Software
  /// Input Sensor Aussenlufttemperatur.
  static constexpr uint8_t PinTemp1OneWireBus  = 30;
  /// Input Sensor Zulufttemperatur.
  static constexpr uint8_t PinTemp2OneWireBus  = 31;
  /// Input Sensor Ablufttemperatur.
  static constexpr uint8_t PinTemp3OneWireBus  = 32;
  /// Input Sensor Fortlufttemperatur.
  static constexpr uint8_t PinTemp4OneWireBus  = 33;

  /// Analog Pin für VOC Sensor.
  static constexpr uint8_t PinVocSensor        = A15;
  /// CO2 Sensor (Winsen MH-Z14) wird über die Zweite Serielle Schnittstelle (Serial2) angeschlossen.
  // Serial2 nutzt beim Arduino Mega Pin 16 u 17
  static constexpr HardwareSerial& SerialMHZ14 = Serial2;

  // ******************************************* E N D E   A N S C H L U S S E I N S T E L L U N G E N **************************************************

  // ************************************** A N S T E U E R U N G   D E R    R E L A I S ****************************************************************
  // Für die Lüfter und den Sommer-Bypass können bis zu vier Relais verbaut sein.
  // Ohne Sommer-Bypass kann die Schaltung auch ohne Relais betrieben werden.
  // Da verschiedene Relais unterschiedlich geschaltet werden, kann hier die logische
  // Schaltung definiert werden (LOW, HIGH oder OPEN für high-impedance mode).

  /// Relay pin digital output needed to close the relay (HIGH/LOW/OPEN).
  static constexpr int8_t RelayON = LOW;
  /// Relay pin digital output needed to open the relay (HIGH/LOW/OPEN).
  static constexpr int8_t RelayOFF = HIGH;

  // ************************************** E N D E   A N S T E U E R U N G   D E R    R E L A I S ******************************************************

  // ************************************** M Q T T   R E P O R T I N G *********************************************************************************

  /// Period for sending heartbeat messages.
  static constexpr uint8_t HeartbeatPeriod = 30;

  /// Send timestamp as heartbeat.
  static constexpr bool HeartbeatTimestamp = false;

  /// At most how often to send temperature messages via MQTT, in seconds.
  static constexpr uint8_t MinIntervalMqttTemp = 5;
  /// At least how often to send temperature messages via MQTT, in seconds.
  static constexpr uint8_t MaxIntervalMqttTemp = 60;
  /// Minimum change in temperature to report per MQTT.
  static constexpr double MinDiffMqttTemp = 0.1;

  /// Default for retain last measurements reading in the MQTT broker.
  static constexpr bool RetainMeasurements = true;

  /// Retain last temperature reading in the MQTT broker.
  static const bool RetainTemperature;

  /// Retain last additional sensors reading in the MQTT broker.
  static const bool RetainAdditionalSensors;

  /// Retain last fan mode reading in the MQTT broker.
  static const bool RetainFanMode;

  /// Retain last fan speed reading in the MQTT broker.
  static const bool RetainFanSpeed;

  /// Retain last antifreeze state reading in the MQTT broker.
  static const bool RetainAntifreezeState;

  /// Retain last bypass state reading in the MQTT broker.
  static const bool RetainBypassState;

  /// Retain last bypass configuration state reading in the MQTT broker.
  static const bool RetainBypassConfigState;

  /// Retain last status bits readings in the MQTT broker.
  static const bool RetainStatusBits;

  // ************************************** E N D E   M Q T T   R E P O R T I N G ***********************************************************************

  // ***************************************************  D E B U G E I N S T E L L U N G E N ********************************************************
  // Allgemeine Debugausgaben auf der seriellen Schnittstelle aktiviert.
  static constexpr bool serialDebug = false;
  // Debugausgaben für die Lüfter auf der seriellen Schnittstelle aktiviert.
  static constexpr bool serialDebugFan = false;
  // Debugausgaben für die Antifreezeschaltung auf der seriellen Schnittstelle aktiviert.
  static constexpr bool serialDebugAntifreeze = false;
  // Debugausgaben für die Summerbypassschaltung auf der seriellen Schnittstelle aktiviert.
  static constexpr bool serialDebugSummerbypass = false;
  // Debugausgaben für die Displayanzeige.
  static constexpr bool serialDebugDisplay = false;
  // Debugausgaben für die Sensoren.
  static constexpr bool serialDebugSensor = false;
  // Debugausgaben für das Programm.
  static constexpr bool serialDebugProgram = false;
  // *******************************************E N D E ***  D E B U G E I N S T E L L U N G E N *****************************************************
};

template<typename FinalConfig>
const IPAddressLiteral KWLDefaultConfig<FinalConfig>::NetworkGateway = (FinalConfig::NetworkIPAddress & FinalConfig::NetworkSubnetMask) | IPAddressLiteral(0, 0, 0, 1);
template<typename FinalConfig>
const IPAddressLiteral KWLDefaultConfig<FinalConfig>::NetworkDNSServer = FinalConfig::NetworkGateway;
template<typename FinalConfig>
const IPAddressLiteral KWLDefaultConfig<FinalConfig>::NetworkNTPServer = FinalConfig::NetworkGateway;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainTemperature = FinalConfig::RetainMeasurements;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainAdditionalSensors = FinalConfig::RetainMeasurements;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainFanMode = FinalConfig::RetainMeasurements;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainFanSpeed = FinalConfig::RetainMeasurements;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainAntifreezeState = FinalConfig::RetainMeasurements;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainBypassState = FinalConfig::RetainMeasurements;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainBypassConfigState = FinalConfig::RetainMeasurements;
template<typename FinalConfig>
const bool KWLDefaultConfig<FinalConfig>::RetainStatusBits = FinalConfig::RetainMeasurements;

/*!
 * @brief Helper macro to add configuration for simple parameters in UserConfig.h.
 *
 * If you get an error here, this means that you probably mistyped a configuration parameter name
 * from KWLDefaultConfig or the value is not compatible for the data type.
 *
 * @param name configuration parameter name.
 * @param value value for the parameter.
 */
#define CONFIGURE(name, value) static constexpr decltype(KWLDefaultConfig<KWLConfig>::name) name = value;

/*!
 * @brief Helper macro to add configuration for object parameters in UserConfig.h.
 *
 * If you get an error here, this means that you probably mistyped a configuration parameter name
 * from KWLDefaultConfig or the value is not compatible for the data type.
 *
 * @param name configuration parameter name.
 * @param values constructor values (in form (a, b, c), including braces).
 */
#define CONFIGURE_OBJ(name, values) static constexpr decltype(KWLDefaultConfig<KWLConfig>::name) name = decltype(KWLDefaultConfig<KWLConfig>::name) values;

/// Actual configuration, including user-specific values.
class KWLConfig : public KWLDefaultConfig<KWLConfig>
{
public:
#if !__has_include("UserConfig.h")
#pragma message("No UserConfig.h provided, using only defaults. Most likely you want local configuration. See KWLConfig.h for instructions.")
#else
#include "UserConfig.h"
#endif
};

template<typename FinalConfig>
constexpr double KWLDefaultConfig<FinalConfig>::StandardKwlModeFactor[MAX_FAN_MODE_CNT];
