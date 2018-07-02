/*
  ################################################################
  #
  #   Copyright notice
  #
  #   Control software for a Room Ventilation System
  #   https://github.com/svenjust/room-ventilation-system
  #
  #   Copyright (C) 2018  Sven Just (sven@familie-just.de)
  #
  #   This program is free software: you can redistribute it and/or modify
  #   it under the terms of the GNU General Public License as published by
  #   the Free Software Foundation, either version 3 of the License, or
  #   (at your option) any later version.
  #
  #   This program is distributed in the hope that it will be useful,
  #   but WITHOUT ANY WARRANTY; without even the implied warranty of
  #   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  #   GNU General Public License for more details.
  #
  #   You should have received a copy of the GNU General Public License
  #   along with this program.  If not, see <https://www.gnu.org/licenses/>.
  #
  #   This copyright notice MUST APPEAR in all copies of the script!
  #
  ################################################################
*/


/*
  # Steuerung einer Lüftungsanlage für Wohnhäuser

  Diese Steuerung wurde als Ersatz der Originalsteuerung entwickelt.

  Das Original war Pluggit P300. Diese Steuerung funktioniert ebenso für P450 und viele weitere Lüftungsanlagen.

  Es werden:
  1. zwei Lüfter angesteuert und deren Tachosignale ausgelesen
  2. vier Temperatursensoren abgefragt
  3. Steuerung Abluftventilator zum Frostschutz
  4. Optional: Vorheizregister als Frostschutz
  5. Optional: Sommer-Bypass geschaltet
  6. Optional: Vorheizregister als Frostschutz
  Die Drehzahlregelung der Lüftermotoren erfolgt durch PID-Regler. Dies sorgt dafür, das die geforderten Drehzahlen der Lüfter sicher erreicht werden. Die Leistungssteuerung des Vorheizregisters erfolgt durch einen PID-Regler.

  Differenzdrucksensoren werden bisher nicht berücksichtigt.

  Die Steuerung ist per LAN (W5100) erreichbar. Als Protokoll wird mqtt verwendet. Über mqtt liefert die Steuerung Temperaturen, Drehzahlen der Lüfter, Stellung der Bypassklappe und den Status der AntiFreeze Funktion zurück.

  Codeteile stammen von:
  - Tachosignal auslesen wie
  https://forum.arduino.cc/index.php?topic=145226.msg1102102#msg1102102
*/

#include <Adafruit_GFX.h>       // TFT
#include <MCUFRIEND_kbv.h>      // TFT
#include <TouchScreen.h>        // Touch
#include <EEPROM.h>             // Speicherung von Einstellungen
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>       // mqtt Client
#include <PID_v1.h>             // PID-Regler für die Drehzahlregelung
#include <OneWire.h>            // OneWire Temperatursensoren
#include <DallasTemperature.h>  // https://www.milesburton.com/Dallas_Temperature_Control_Library
#include <Wire.h>
#include <DHT.h>
#include <DHT_U.h>

#include "FanRPM.h"
#include "MultiPrint.h"
#include "MQTTClient.h"
#include "Relay.h"

#include "kwl_config.h"
#include "MQTTTopic.hpp"

// ***************************************************  V E R S I O N S N U M M E R   D E R    S W   *************************************************
#define strVersion "v0.15"


// ***************************************************  A N S T E U E R U N G   P W M oder D A C   ***************************************************
#define  ControlFansDAC         1 // 1 = zusätzliche Ansteuerung durch DAC über SDA und SLC (und PWM)
// ****************************************** E N D E   A N S T E U E R U N G   P W M oder D A C   ***************************************************


// ***************************************************  T T F   U N D   T O U C H  ********************************************************
// *** TFT
// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GREY    0x7BEF
uint16_t ID;
MCUFRIEND_kbv tft;
// Definition for Touch
// most mcufriend shields use these pins and Portrait mode:
uint8_t YP = A1;  // must be an analog pin, use "An" notation!
uint8_t XM = A2;  // must be an analog pin, use "An" notation!
uint8_t YM = 7;   // can be a digital pin
uint8_t XP = 6;   // can be a digital pin
uint8_t SwapXY = 0;

uint16_t TS_LEFT = 949;
uint16_t TS_RT  = 201;
uint16_t TS_TOP = 943;
uint16_t TS_BOT = 205;
const char *name = "Unknown controller";

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
TSPoint tp;
#define MINPRESSURE 20
#define MAXPRESSURE 1000
#define SWAP(a, b) {uint16_t tmp = a; a = b; b = tmp;}
// ***************************************************  E N D E   T T F   U N D   T O U C H  ********************************************************


Relay relBypassPower(kwl_config::PinBypassPower);
Relay relBypassDirection(kwl_config::PinBypassDirection);
Relay relFan1Power(kwl_config::PinFan1Power);
Relay relFan2Power(kwl_config::PinFan2Power);

// Sind die folgenden Variablen auf true, werden beim nächsten Durchlauf die entsprechenden mqtt Messages gesendet,
// anschliessend wird die Variable wieder auf false gesetzt
boolean mqttCmdSendTemp                        = false;
boolean mqttCmdSendDht                         = false;
boolean mqttCmdSendMHZ14                       = false;
boolean mqttCmdSendFans                        = false;
boolean mqttCmdSendBypassState                 = false;
boolean mqttCmdSendBypassAllValues             = false;
boolean mqttCmdSendMode                        = false;
boolean mqttCmdSendConfig                      = false;
// mqttDebug Messages
boolean mqttCmdSendAlwaysDebugFan1             = true;
boolean mqttCmdSendAlwaysDebugFan2             = true;
boolean mqttCmdSendAlwaysDebugPreheater        = true;

boolean EffiencyCalcNow = false;

char   TEMPChar[10];            // Hilfsvariable zu Konvertierung
char   buffer[7];               // the ASCII of the integer will be stored in this char array
String TEMPAsString;            // Ausgelesene Wert als String
String ErrorText;               // Textvariable für Fehlermeldung
String InfoText;                // Textvariable für Infomeldung

// Variablen für Lüfter Tacho
#define CalculateSpeed_PID             1
#define CalculateSpeed_PROP            0
int FansCalculateSpeed                = CalculateSpeed_PROP;   // 0 = Berechne das PWM Signal Proportional zur Nenndrehzahl der Lüfter; 1=PID-Regler verwenden
double speedTachoFan1                 = 0;    // Zuluft U/min
double speedTachoFan2                 = 0;    // Abluft U/min
double SendMqttSpeedTachoFan1         = 0;
double SendMqttSpeedTachoFan2         = 0;

FanRPM fan1speed;
FanRPM fan2speed;

double StandardSpeedSetpointFan1      = kwl_config::StandardSpeedSetpointFan1;      // Solldrehzahl im U/min für Zuluft bei kwlMode = 2 (Standardlüftungsstufe), Drehzahlen werden aus EEPROM gelesen.
double StandardSpeedSetpointFan2      = kwl_config::StandardSpeedSetpointFan2;      // Solldrehzahl im U/min für Abluft bei kwlMode = 2 (Standardlüftungsstufe)
double speedSetpointFan1              = 0;                                 // Solldrehzahl im U/min für Zuluft bei Berücksichtungs der Lüftungsstufe
double speedSetpointFan2              = 0;                                 // Solldrehzahl im U/min für Zuluft bei Berücksichtungs der Lüftungsstufe
double techSetpointFan1               = 0;                                 // PWM oder Analogsignal 0..1000 für Zuluft
double techSetpointFan2               = 0;                                 // PWM oder Analogsignal 0..1000 für Abluft
// Ende Variablen für Lüfter

int  PwmSetpointFan1[kwl_config::StandardModeCnt];                                  // Speichert die pwm-Werte für die verschiedenen Drehzahlen
int  PwmSetpointFan2[kwl_config::StandardModeCnt];

#define FanMode_Normal                 0
#define FanMode_Calibration            1
int FanMode                           = FanMode_Normal;                   // Umschaltung zum Kalibrieren der PWM Signale zur Erreichung der Lüfterdrehzahlen für jede Stufe
int actKwlMode = 0;
int kwlMode = kwl_config::StandardKwlMode;

// Start Definition für Heating Appliance (Feuerstätte)  //////////////////////////////////////////
int            heatingAppCombUse                    = kwl_config::StandardHeatingAppCombUse;
unsigned long  heatingAppCombUseAntiFreezeInterval  = 14400000;   // 4 Stunden = 4 *60 * 60 * 1000
unsigned long  heatingAppCombUseAntiFreezeStartTime = 0;
// Ende  Definition für Heating Appliance (Feuerstätte)  //////////////////////////////////////////

// Start Definition für AntiFreeze (Frostschutz) //////////////////////////////////////////
#define antifreezeOff                   0
#define antifreezePreheater             1
#define antifreezeZuluftOff             2
#define antifreezeFireplace             3

int     antifreezeState               = antifreezeOff;
float   antifreezeTemp                = 1.5;     // Nach kaltem Wetter im Feb 2018 gemäß Messwerte
int     antifreezeHyst                = 2;
double  antifreezeTempUpperLimit;
boolean antifreezeAlarm               = false;

double        techSetpointPreheater   = 0.0;      // Analogsignal 0..1000 für Vorheizer
unsigned long PreheaterStartMillis    = 0;        // Beginn der Vorheizung
// Ende Definition für AntiFreeze (Frostschutz) //////////////////////////////////////////

// Start - Variablen für Bypass ///////////////////////////////////////////
#define bypassMode_Auto         0
#define bypassMode_Manual       1
#define bypassFlapState_Unknown 0
#define bypassFlapState_Close   1
#define bypassFlapState_Open    2

int  bypassManualSetpoint        = kwl_config::StandardBypassManualSetpoint;   // Standardstellung Bypass
int  bypassMode                  = kwl_config::StandardBypassMode;             // Automatische oder Manuelle Steuerung der Bypass-Klappe
int  bypassFlapState             = bypassFlapState_Unknown;           // aktuelle Stellung der Bypass-Klappe
int  bypassFlapStateDriveRunning = bypassFlapState_Unknown;
int  bypassTempAbluftMin         = kwl_config::StandardBypassTempAbluftMin;
int  bypassTempAussenluftMin     = kwl_config::StandardBypassTempAussenluftMin;
int  bypassHystereseMinutes      = kwl_config::StandardBypassHystereseMinutes;
int  bypassFlapSetpoint          = kwl_config::StandardBypassManualSetpoint;

unsigned long bypassLastChangeMillis   = 0;                       // Letzte Änderung für Hysterese
long          bypassFlapsDriveTime = 120000; // 120 * 1000;       // Fahrzeit (ms) der Klappe zwischen den Stellungen Open und Close
boolean       bypassFlapsRunning = false;                         // True, wenn Klappe fährt
unsigned long bypassFlapsStartTime = 0;                           // Startzeit für den Beginn Klappenwechsels
// Ende  - Variablen für Bypass ///////////////////////////////////////////


// Definitionen für das Scheduling
unsigned long intervalTachoFan               = 950;
unsigned long intervalSetFan                 = 1000;
unsigned long intervalTempRead               = 5000;    // Abfrage Temperatur, muss größer als 1000 sein

unsigned long intervalEffiencyCalc           = 5000;    // Berechnung Wirkungsgrad
unsigned long intervalAntifreezeCheck        = 60000;   //  60000 = 60 * 1000         // Frostschutzprüfung je Minute
unsigned long intervalAntiFreezeAlarmCheck   = 600000;  // 600000 = 10 * 60 * 1000;   // 10 Min Zeitraum zur Überprüfung, ob Vorheizregister die Temperatur erhöhen kann,
unsigned long intervalBypassSummerCheck      = 60000;   // Zeitraum zum Check der Bedingungen für BypassSummerCheck, 1 Minuten
unsigned long intervalBypassSummerSetFlaps   = 60000;   // 60000;  // 1 * 60 * 1000 Zeitraum zum Fahren des Bypasses, 1 Minuten
unsigned long intervalCheckForErrors         = 1000;
unsigned long intervalDHTRead                = 10000;
unsigned long intervalMHZ14Read              = 10000;
unsigned long intervalTGS2600Read            = 30000;

unsigned long intervalMqttFan                = 5000;
unsigned long intervalMqttMode               = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttTemp               = 5000;
unsigned long intervalMqttMHZ14              = 60000;
unsigned long intervalMqttTempOversampling   = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttFanOversampling    = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttMHZ14Oversampling  = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttBypassState        = 900000; //15 * 60 * 1000; 15 Minuten

unsigned long previousMillisFan                   = 0;
unsigned long previousMillisSetFan                = 0;
unsigned long previousMillisEffiencyCalc          = 0;
unsigned long previousMillisAntifreeze            = 0;
unsigned long previousMillisBypassSummerCheck     = 0;
unsigned long previousMillisBypassSummerSetFlaps  = 0;
unsigned long previousMillisCheckForErrors        = 0;
unsigned long previousMillisDHTRead               = 0;
unsigned long previousMillisMHZ14Read             = 0;
unsigned long previousMillisTGS2600Read           = 0;

unsigned long previousMillisMqttHeartbeat         = 0;
unsigned long previousMillisMqttFan               = 0;
unsigned long previousMillisMqttMode              = 0;
unsigned long previousMillisMqttFanOversampling   = 0;
unsigned long previousMillisMqttTemp              = 0;
unsigned long previousMillisMqttTempOversampling  = 0;
unsigned long previousMillisMqttDht               = 0;
unsigned long previousMillisMqttDhtOversampling   = 0;
unsigned long previousMillisMqttMHZ14             = 0;
unsigned long previousMillisMqttMHZ14Oversampling = 0;
unsigned long previousMillisMqttBypassState       = 0;

unsigned long previous100Millis                   = 0;
unsigned long previousMillisNtpTime               = 0;
unsigned long previousMillisTemp                  = 0;
unsigned long currentMillis                       = 0;


// Begin EEPROM
const int  BUFSIZE = 50;
char       eeprombuffer[BUFSIZE];
const int  EEPROM_MIN_ADDR = 0;
const int  EEPROM_MAX_ADDR = 1023;
// Ende EEPROM


EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
boolean bLanOk     = false;
boolean bMqttOk    = false;

/// Init tracer which prints to both TFT and Serial.
static MultiPrint initTracer(Serial, tft);
/// Task scheduler
static Scheduler scheduler;
/// Global MQTT client.
// TODO move all MQTT service handling into the client
static MQTTClient mqttClientWrapper(mqttClient);
/// Set of temperature sensors
static TempSensors tempSensors(scheduler, mqttClient, initTracer);

unsigned long lastMqttReconnectAttempt    = 0;
unsigned long lastLanReconnectAttempt = 0;

double TEMP1_Aussenluft = -127.0;    // Temperatur Außenluft
double TEMP2_Zuluft     = -127.0;    // Temperatur Zuluft
double TEMP3_Abluft     = -127.0;    // Temperatur Abluft
double TEMP4_Fortluft   = -127.0;    // Temperatur Fortluft
int    EffiencyKwl      =  100 ;     // Wirkungsgrad auf Differenzberechnung der Temps

// PID REGLER
// Define the aggressive and conservative Tuning Parameters
// Nenndrehzahl Lüfter 3200, Stellwert 0..1000 entspricht 0-10V
double aggKp  = 0.5,  aggKi = 0.1, aggKd  = 0.001;
double consKp = 0.1, consKi = 0.1, consKd = 0.001;
double heaterKp = 50, heaterKi = 0.1, heaterKd = 0.025;

//Specify the links and initial tuning parameters
PID PidFan1(&speedTachoFan1, &techSetpointFan1, &speedSetpointFan1, consKp, consKi, consKd, P_ON_M, DIRECT );
PID PidFan2(&speedTachoFan2, &techSetpointFan2, &speedSetpointFan2, consKp, consKi, consKd, P_ON_M, DIRECT );
PID PidPreheater(&TEMP4_Fortluft, &techSetpointPreheater, &antifreezeTempUpperLimit, heaterKp, heaterKi, heaterKd, P_ON_M, DIRECT);
///////////////////////

// Temperatur Sensoren, Pinbelegung steht oben
#define TEMPERATURE_PRECISION 0x1F            // Genauigkeit der Temperatursensoren 9_BIT, Standard sind 12_BIT
OneWire Temp1OneWire(kwl_config::PinTemp1OneWireBus);     // Einrichten des OneWire Bus um die Daten der Temperaturfühler abzurufen
OneWire Temp2OneWire(kwl_config::PinTemp2OneWireBus);
OneWire Temp3OneWire(kwl_config::PinTemp3OneWireBus);
OneWire Temp4OneWire(kwl_config::PinTemp4OneWireBus);
DallasTemperature Temp1Sensor(&Temp1OneWire); // Bindung der Sensoren an den OneWire Bus
DallasTemperature Temp2Sensor(&Temp2OneWire);
DallasTemperature Temp3Sensor(&Temp3OneWire);
DallasTemperature Temp4Sensor(&Temp4OneWire);

//
float SendMqttTEMP1 = 0;    // Temperatur Außenluft, gesendet per Mqtt
float SendMqttTEMP2 = 0;    // Temperatur Zuluft
float SendMqttTEMP3 = 0;    // Temperatur Abluft
float SendMqttTEMP4 = 0;    // Temperatur Fortluft

float SendMqttDHT1Temp = 0;    // Temperatur Außenluft, gesendet per Mqtt
float SendMqttDHT1Hum  = 0;    // Temperatur Zuluft
float SendMqttDHT2Temp = 0;    // Temperatur Abluft
float SendMqttDHT2Hum  = 0;    // Temperatur Fortluft

int   SendMqttMHZ14CO2 = 0;    // CO2 Wert


// DHT Sensoren
DHT_Unified dht1(kwl_config::PinDHTSensor1, DHT22);
DHT_Unified dht2(kwl_config::PinDHTSensor2, DHT22);
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

// Ende Definition
///////////////////////////////////////


void mqttReceiveMsg(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print(F("Message arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));
  Serial.write(payload, length);
  Serial.println();
  String topicStr = topic;

  // Set Values
  if (topicStr == MQTTTopic::CmdFansCalculateSpeedMode) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "PROP")  {
      FansCalculateSpeed = CalculateSpeed_PROP;
    }
    if (s == "PID") {
      FansCalculateSpeed = CalculateSpeed_PID;
    }
  }
  if (topicStr == MQTTTopic::CmdCalibrateFans) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "YES")   {
      Serial.println(F("Kalibrierung Lüfter wird gestartet"));
      SpeedCalibrationStart();
    }
  }
  else if (topicStr == MQTTTopic::CmdResetAll) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "YES")   {
      Serial.println(F("Speicherbereich wird gelöscht"));
      initializeEEPROM(true);
      // Reboot
      Serial.println(F("Reboot"));
      delay (1000);
      asm volatile ("jmp 0");
    }
  }
  else if (topicStr == MQTTTopic::CmdFan1Speed) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    StandardSpeedSetpointFan1 = i;
    // Drehzahl Lüfter 1
    eeprom_write_int(2, i);
  }
  else if (topicStr == MQTTTopic::CmdFan2Speed) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    StandardSpeedSetpointFan2 = i;
    // Drehzahl Lüfter 1
    eeprom_write_int(4, i);
  }
  else if (topicStr == MQTTTopic::CmdMode) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    if (i <= kwl_config::StandardModeCnt)  kwlMode = i;
    mqttCmdSendMode = true;
    // KWL Stufe
  }
  else if (topicStr == MQTTTopic::CmdAntiFreezeHyst) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    antifreezeHyst = i;
    antifreezeTempUpperLimit = antifreezeTemp + antifreezeHyst;
    // AntiFreezeHysterese
    eeprom_write_int(12, i);
  }
  else if (topicStr == MQTTTopic::CmdBypassHystereseMinutes) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    bypassHystereseMinutes = i;
    // BypassHystereseMinutes
    eeprom_write_int(10, i);
  }
  else if (topicStr == MQTTTopic::CmdBypassManualFlap) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "open")  {
      bypassManualSetpoint = bypassFlapState_Open;
    }
    if (s == "close") {
      bypassManualSetpoint = bypassFlapState_Close;
    }
    // Stellung Bypassklappe bei manuellem Modus
  }
  else if (topicStr == MQTTTopic::CmdBypassMode) {
    // Auto oder manueller Modus
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "auto")   {
      bypassMode = bypassMode_Auto;
      mqttCmdSendBypassState = true;
    }
    if (s == "manual") {
      bypassMode = bypassMode_Manual;
      mqttCmdSendBypassState = true;
    }
  }
  else if (topicStr == MQTTTopic::CmdHeatingAppCombUse) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "YES")   {
      Serial.println(F("Feuerstättenmodus wird aktiviert und gespeichert"));
      int i = 1;
      heatingAppCombUse = i;
      eeprom_write_int(60, i);
      mqttCmdSendTemp = true;
    }
    if (s == "NO")   {
      Serial.println(F("Feuerstättenmodus wird DEAKTIVIERT und gespeichert"));
      int i = 0;
      heatingAppCombUse = i;
      eeprom_write_int(60, i);
      mqttCmdSendTemp = true;
    }
  }

  // Get Commands
  else if (topicStr == MQTTTopic::CmdGetTemp) {
    payload[length] = '\0';
    String s = String((char*)payload);
    mqttCmdSendTemp = true;
  }
  else if (topicStr == MQTTTopic::CmdGetSpeed) {
    payload[length] = '\0';
    String s = String((char*)payload);
    mqttCmdSendFans = true;
  }
  else if (topicStr == MQTTTopic::CmdBypassGetValues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    mqttCmdSendBypassAllValues = true;
  }
  else if (topicStr == MQTTTopic::CmdGetvalues) {
    // Alle Values
    payload[length] = '\0';
    String s = String((char*)payload);
    mqttCmdSendTemp            = true;
    mqttCmdSendDht             = true;
    mqttCmdSendFans            = true;
    mqttCmdSendBypassAllValues = true;
    mqttCmdSendMHZ14           = true;
  }

  // Debug Messages, den folgenden Block in der produktiven Version auskommentieren
  else if (topicStr == MQTTTopic::KwlDebugsetTemperaturAussenluft) {
    payload[length] = '\0';
    TEMP1_Aussenluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }
  else if (topicStr == MQTTTopic::KwlDebugsetTemperaturZuluft) {
    payload[length] = '\0';
    TEMP2_Zuluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }
  else if (topicStr == MQTTTopic::KwlDebugsetTemperaturAbluft) {
    payload[length] = '\0';
    TEMP3_Abluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }
  else if (topicStr == MQTTTopic::KwlDebugsetTemperaturFortluft) {
    payload[length] = '\0';
    TEMP4_Fortluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }
  else if (topicStr == MQTTTopic::KwlDebugsetFan1Getvalues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "on")   {
      mqttCmdSendAlwaysDebugFan1 = true;
    }
    if (s == "off")  {
      mqttCmdSendAlwaysDebugFan1 = false;
    }
  }
  else if (topicStr == MQTTTopic::KwlDebugsetFan2Getvalues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "on")   {
      mqttCmdSendAlwaysDebugFan2 = true;
    }
    if (s == "off")  {
      mqttCmdSendAlwaysDebugFan2 = false;
    }
  }
  else if (topicStr == MQTTTopic::KwlDebugsetFan1PWM) {
    // update PWM value for the current state
    payload[length] = '\0';
    if (kwlMode != 0) {
      int value = String((char*)payload).toInt();
      if (value < 0)
        value = 0;
      if (value > 1000)
        value = 1000;
      PwmSetpointFan1[kwlMode] = value;
      setSpeedToFan();
    }
  }
  else if (topicStr == MQTTTopic::KwlDebugsetFan2PWM) {
    // update PWM value for the current state
    payload[length] = '\0';
    if (kwlMode != 0) {
      int value = String((char*)payload).toInt();
      if (value < 0)
        value = 0;
      if (value > 1000)
        value = 1000;
      PwmSetpointFan2[kwlMode] = value;
      setSpeedToFan();
    }
  }
  else if (topicStr == MQTTTopic::KwlDebugsetFanPWMStore) {
    // store calibration data in EEPROM
    for (int i = 0; ((i < kwl_config::StandardModeCnt) && (i < 10)); i++) {
      eeprom_write_int(20 + (i * 4), PwmSetpointFan1[i]);
      eeprom_write_int(22 + (i * 4), PwmSetpointFan2[i]);
    }
  }
  // Debug Messages, bis hier auskommentieren
  else {
    // forward to new MQTT handler
    mqttClientWrapper.mqttReceiveMsg(topic, payload, length);
  }

}

boolean mqttReconnect() {
  Serial.println (F("reconnect start"));
  Serial.println ((long)currentMillis);
  if (mqttClient.connect("arduinoClientKwl", MQTTTopic::Heartbeat, 0, true, "offline")) {
    // Once connected, publish an announcement...
    mqttClient.publish(MQTTTopic::Heartbeat, "online");
    // ... and resubscribe
    mqttClient.subscribe(MQTTTopic::Command);
    mqttClient.subscribe(MQTTTopic::CommandDebug);
  }
  Serial.println (F("reconnect end"));
  Serial.println ((long)currentMillis);
  Serial.print (F("IsMqttConnected: "));
  Serial.println (mqttClient.connected());
  return mqttClient.connected();
}

void setSpeedToFan() {
  // Ausgabewert für Lüftersteuerung darf zwischen 0-10V liegen, dies entspricht 0..1023, vereinfacht 0..1000
  // 0..1000 muss umgerechnet werden auf 0..255 also durch 4 geteilt werden
  // max. Lüfterdrehzahl bei Pabstlüfter 3200 U/min
  // max. Drehzahl 2300 U/min bei Testaufbau (alte Prozessorlüfter)

  speedSetpointFan1 = StandardSpeedSetpointFan1 * kwl_config::StandardKwlModeFactor[kwlMode];
  speedSetpointFan2 = StandardSpeedSetpointFan2 * kwl_config::StandardKwlModeFactor[kwlMode];

  double gap1 = abs(speedSetpointFan1 - speedTachoFan1); //distance away from setpoint
  double gap2 = abs(speedSetpointFan2 - speedTachoFan2); //distance away from setpoint

  // Das PWM-Signal kann entweder per PID-Regler oder unten per Dreisatz berechnen werden.
  if (FansCalculateSpeed == CalculateSpeed_PID) {
    if (gap1 < 1000) {
      PidFan1.SetTunings(consKp, consKi, consKd);
    }  else   {
      PidFan1.SetTunings(aggKp, aggKi, aggKd);
    }
    if (gap2 < 1000) {
      PidFan2.SetTunings(consKp, consKi, consKd);
    }  else   {
      PidFan2.SetTunings(aggKp, aggKi, aggKd);
    }
    PidFan1.Compute();
    PidFan2.Compute();

  } else if (FansCalculateSpeed == CalculateSpeed_PROP) {
    techSetpointFan1 = PwmSetpointFan1[kwlMode];
    techSetpointFan2 = PwmSetpointFan2[kwlMode];
  }

  if (kwlMode == 0)               {
    techSetpointFan1 = 0 ;  // Lüfungsstufe 0 alles ausschalten
    techSetpointFan2 = 0;
  }

  DoActionAntiFreezeState();
  SetPreheater();

  // Grenzwertbehandlung: Max- / Min-Werte
  if (techSetpointFan1 < 0)    techSetpointFan1 = 0;
  if (techSetpointFan2 < 0)    techSetpointFan2 = 0;
  if (techSetpointFan1 > 1000) techSetpointFan1 = 1000;
  if (techSetpointFan2 > 1000) techSetpointFan2 = 1000;


  if (mqttCmdSendAlwaysDebugFan1) {
    mqtt_debug_fan1();
  }
  if (mqttCmdSendAlwaysDebugFan2) {
    mqtt_debug_fan2();
  }

  if (kwl_config::serialDebugFan == 1) {
    Serial.print (F("Timestamp: "));
    Serial.println ((long)millis());
    Serial.print (F("Fan 1: "));
    Serial.print (F("\tGap: "));
    Serial.print (speedTachoFan1 - speedSetpointFan1);
    Serial.print (F("\tspeedTachoFan1: "));
    Serial.print (speedTachoFan1);
    Serial.print (F("\ttechSetpointFan1: "));
    Serial.print (techSetpointFan1);
    Serial.print (F("\tspeedSetpointFan1: "));
    Serial.println(speedSetpointFan1);
    fan1speed.dump();

    Serial.print (F("Fan 2: "));
    Serial.print (F("\tGap: "));
    Serial.print (speedTachoFan2 - speedSetpointFan2);
    Serial.print (F("\tspeedTachoFan2: "));
    Serial.print (speedTachoFan2);
    Serial.print (F("\ttechSetpointFan2: "));
    Serial.print (techSetpointFan2);
    Serial.print (F("\tspeedSetpointFan2: "));
    Serial.println(speedSetpointFan2);
    fan2speed.dump();
  }
  // Setzen per PWM
  analogWrite(kwl_config::PinFan1PWM, techSetpointFan1 / 4);
  analogWrite(kwl_config::PinFan2PWM, techSetpointFan2 / 4);

  // Setzen der Werte per DAC
  if (ControlFansDAC == 1) {
    byte HBy;
    byte LBy;

    // FAN 1
    HBy = techSetpointFan1 / 256;        //HIGH-Byte berechnen
    LBy = techSetpointFan1 - HBy * 256;  //LOW-Byte berechnen
    Wire.beginTransmission(kwl_config::DacI2COutAddr); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(kwl_config::DacChannelFan1);             // FAN 1 schreiben
    Wire.write(LBy);                          // LOW-Byte schreiben
    Wire.write(HBy);                          // HIGH-Byte schreiben
    Wire.endTransmission();                   // Ende

    // FAN 2
    HBy = techSetpointFan2 / 256;        //HIGH-Byte berechnen
    LBy = techSetpointFan2 - HBy * 256;  //LOW-Byte berechnen
    Wire.beginTransmission(kwl_config::DacI2COutAddr); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(kwl_config::DacChannelFan2);             // FAN 2 schreiben
    Wire.write(LBy);                          // LOW-Byte schreiben
    Wire.write(HBy);                          // HIGH-Byte schreiben
    Wire.endTransmission();                   // Ende
  }

}

void SetPreheater() {
  // Das Vorheizregister wird durch ein PID geregelt
  // Die Stellgröße darf zwischen 0..10V liegen
  // Wenn der Zuluftventilator unter einer Schwelle des Tachosignals liegt, wird das Vorheizregister IMMER ausgeschaltet (SICHERHEIT)
  // Schwelle: 1000 U/min

  if (kwl_config::serialDebugAntifreeze == 1) {
    Serial.println (F("SetPreheater start"));
  }

  // Sicherheitsabfrage
  if (speedTachoFan1 < 600 || (techSetpointFan1 == 0) )
  { // Sicherheitsabschaltung Vorheizer unter 600 Umdrehungen Zuluftventilator
    techSetpointPreheater = 0;
  }
  if (mqttCmdSendAlwaysDebugPreheater) {
    mqtt_debug_Preheater();
  }
  // Setzen per PWM
  analogWrite(kwl_config::PinPreheaterPWM, techSetpointPreheater / 4);

  // Setzen der Werte per DAC
  byte HBy;
  byte LBy;

  // Preheater DAC
  HBy = techSetpointPreheater / 256;        //HIGH-Byte berechnen
  LBy = techSetpointPreheater - HBy * 256;  //LOW-Byte berechnen
  Wire.beginTransmission(kwl_config::DacI2COutAddr); // Start Übertragung zur ANALOG-OUT Karte
  Wire.write(kwl_config::DacChannelPreheater);        // PREHEATER schreiben
  Wire.write(LBy);                          // LOW-Byte schreiben
  Wire.write(HBy);                          // HIGH-Byte schreiben
  Wire.endTransmission();                   // Ende
}

void countUpFan1() {
  fan1speed.interrupt();
}
void countUpFan2() {
  fan2speed.interrupt();
}

void loopTemperaturRequest() {
  // Diese Routine startet den Request  bei OneWire Sensoren
  // Dies geschieht 1000mS vor der Abfrage der Temperaturen
  // in loopTemperaturRead. loopTemperaturRead setzt auch
  // previousMillisTemp neu.
  currentMillis = millis();
  if (currentMillis - previousMillisTemp >= intervalTempRead - 1000) {
    Temp1Sensor.requestTemperatures();
    Temp2Sensor.requestTemperatures();
    Temp3Sensor.requestTemperatures();
    Temp4Sensor.requestTemperatures();
  }
}

void loopTemperaturRead() {
  currentMillis = millis();
  if (currentMillis - previousMillisTemp >= intervalTempRead) {
    previousMillisTemp = currentMillis;
    float t;
    t = Temp1Sensor.getTempCByIndex(0); if (t > -127.0) {
      TEMP1_Aussenluft = t;
    }
    t = Temp2Sensor.getTempCByIndex(0); if (t > -127.0) {
      TEMP2_Zuluft = t;
    }
    t = Temp3Sensor.getTempCByIndex(0); if (t > -127.0) {
      TEMP3_Abluft = t;
    }
    t = Temp4Sensor.getTempCByIndex(0); if (t > -127.0) {
      TEMP4_Fortluft = t;
    }
  }
}

void DoActionAntiFreezeState() {
  // Funktion wird ausgeführt, um AntiFreeze (Frostschutz) zu erreichen.

  mqttCmdSendTemp = true;

  switch (antifreezeState) {

    case antifreezePreheater:
      PidPreheater.Compute();
      break;

    case antifreezeZuluftOff:
      // Zuluft aus
      if (kwl_config::serialDebugAntifreeze == 1) Serial.println (F("fan1 = 0"));
      techSetpointFan1 = 0;
      techSetpointPreheater = 0;
      break;

    case  antifreezeFireplace:
      // Feuerstättenmodus
      // beide Lüfter aus
      if (kwl_config::serialDebugAntifreeze == 1) {
        Serial.println (F("fan1 = 0"));
        Serial.println (F("fan2 = 0"));
      }
      techSetpointFan1 = 0;
      techSetpointFan2 = 0;
      techSetpointPreheater = 0;
      break;

    default:
      // Normal Mode without AntiFreeze
      // Vorheizregister aus
      techSetpointPreheater = 0;
      break;
  }
}

void loopAntiFreezeCheck() {
  // Funktion wird regelmäßig zum Überprüfen ausgeführt

  currentMillis = millis();
  if (currentMillis - previousMillisAntifreeze >= intervalAntifreezeCheck) {
    if (kwl_config::serialDebugAntifreeze == 1)  Serial.println (F("loopAntiFreezeCheck start"));
    previousMillisAntifreeze = currentMillis;

    // antifreezeState = aktueller Status der AntiFrostSchaltung
    // Es wird in jeden Status überprüft, ob die Bedingungen für einen Statuswechsel erfüllt sind
    // Wenn ja, werden die einmaligen Aktionen (Setzen von Variablen) hier ausgeführt.
    // siehe auch "/Docs/Programming/Zustandsänderung Antifreeze.jpg"
    // Die regelmäßigen Aktionen für jedem Status werden in DoActionAntiFreezeState ausgeführt.
    switch (antifreezeState) {

      case antifreezeOff:  // antifreezeState = 0
        if ((TEMP4_Fortluft <= antifreezeTemp) && (TEMP1_Aussenluft < 0.0)
            && (TEMP4_Fortluft > -127.0) && (TEMP1_Aussenluft > -127.0))         // Wenn Sensoren fehlen, ist der Wert -127
        {
          // Neuer Status: antifreezePreheater
          antifreezeState = antifreezePreheater;

          PreheaterStartMillis = millis();
          // Vorheizer einschalten
          antifreezeTempUpperLimit  = antifreezeTemp + antifreezeHyst;
          PidPreheater.SetMode(AUTOMATIC);  // Pid einschalten
        }
        break;

      // Vorheizregister ist an
      case  antifreezePreheater:  // antifreezeState = 1

        if (TEMP4_Fortluft > antifreezeTemp + antifreezeHyst) {
          // Neuer Status: antifreezeOff
          antifreezeState = antifreezeOff;
          PidPreheater.SetMode(MANUAL);
        }

        if ((currentMillis - PreheaterStartMillis > intervalAntiFreezeAlarmCheck) &&
            (TEMP4_Fortluft <= antifreezeTemp) && (TEMP1_Aussenluft < 0.0)
            && (TEMP4_Fortluft > -127.0) && (TEMP1_Aussenluft > -127.0)) {
          // 10 Minuten vergangen seit antifreezeState == antifreezePreheater und Temperatur immer noch unter antifreezeTemp
          if (kwl_config::serialDebugAntifreeze == 1) Serial.println (F("10 Minuten vergangen seit antifreezeState == antifreezePreheater"));

          switch (heatingAppCombUse) {
            case 0:
              // Neuer Status: antifreezeZuluftOff
              antifreezeState = antifreezeZuluftOff;
              PidPreheater.SetMode(MANUAL);
              break;
            case 1:
              // Neuer Status: antifreezeFireplace
              antifreezeState =  antifreezeFireplace;
              PidPreheater.SetMode(MANUAL);
              // Zeit speichern
              heatingAppCombUseAntiFreezeStartTime = millis();
              break;
          }
          break;

        // Zuluftventilator ist aus, kein KAMIN angeschlossen
        case antifreezeZuluftOff:  // antifreezeState = 2
          if (TEMP4_Fortluft > antifreezeTemp + antifreezeHyst) {
            // Neuer Status: antifreezeOff
            antifreezeState = antifreezeOff;
            PidPreheater.SetMode(MANUAL);
          }
          break;

        // Zu- und Abluftventilator sind für vier Stunden aus, KAMINMODUS
        case antifreezeFireplace:  // antifreezeState = 4
          if (millis() - heatingAppCombUseAntiFreezeStartTime > heatingAppCombUseAntiFreezeInterval) {
            // Neuer Status: antifreezeOff
            antifreezeState = antifreezeOff;
            PidPreheater.SetMode(MANUAL);
          }
          break;
        }

        if (kwl_config::serialDebugAntifreeze == 1) {
          Serial.print (F("millis: "));
          Serial.println (millis());
          Serial.print (F("antifreezeState: "));
          Serial.println (antifreezeState);
        }
    }
  }
}

void loopBypassSummerCheck() {
  // Bedingungen für Sommer Bypass überprüfen und Variable ggfs setzen
  currentMillis = millis();
  if (currentMillis - previousMillisBypassSummerCheck >= intervalBypassSummerCheck) {
    if (kwl_config::serialDebugSummerbypass == 1) {
      Serial.println(F("BypassSummerCheck"));
    }
    previousMillisBypassSummerCheck = currentMillis;
    // Auto oder Manual?
    if (bypassMode == bypassMode_Auto) {
      if (kwl_config::serialDebugSummerbypass == 1) {
        Serial.println(F("bypassMode_Auto"));
      }
      // Automatic
      // Hysterese überprüfen
      if (currentMillis - bypassLastChangeMillis >= (bypassHystereseMinutes * 60L * 1000L)) {
        if (kwl_config::serialDebugSummerbypass == 1) {
          Serial.println(F("Time to Check"));
          Serial.print(F("TEMP1_Aussenluft       : "));
          Serial.println(TEMP1_Aussenluft);
          Serial.print(F("TEMP3_Abluft           : "));
          Serial.println(TEMP3_Abluft);
          Serial.print(F("bypassTempAbluftMin    : "));
          Serial.println(bypassTempAbluftMin);
          Serial.print(F("bypassTempAussenluftMin: "));
          Serial.println(bypassTempAussenluftMin);
        }
        if ((TEMP1_Aussenluft > -127.0) && (TEMP3_Abluft > -127.0)) {
          if ((TEMP1_Aussenluft    < TEMP3_Abluft - 2)
              && (TEMP3_Abluft     > bypassTempAbluftMin)
              && (TEMP1_Aussenluft > bypassTempAussenluftMin)) {
            //ok, dann Klappe öffen
            if (bypassFlapSetpoint != bypassFlapState_Open) {
              if (kwl_config::serialDebugSummerbypass == 1) {
                Serial.println(F("Klappe öffen"));
              }
              bypassFlapSetpoint = bypassFlapState_Open;
              bypassLastChangeMillis = millis();
            }
          } else {
            //ok, dann Klappe schliessen
            if (bypassFlapSetpoint != bypassFlapState_Close) {
              if (kwl_config::serialDebugSummerbypass == 1) {
                Serial.println(F("Klappe schliessen"));
              }
              bypassFlapSetpoint = bypassFlapState_Close;
              bypassLastChangeMillis = millis();
            }
          }
        }
      }
    } else {
      // Manuelle Schaltung
      if (bypassManualSetpoint != bypassFlapSetpoint) {
        bypassFlapSetpoint = bypassManualSetpoint;
      }
    }
  }
}

void loopBypassSummerSetFlaps() {
  // Klappe gemäß bypassFlapSetpoint setzen
  currentMillis = millis();
  if (currentMillis - previousMillisBypassSummerSetFlaps >= intervalBypassSummerSetFlaps) {
    if (kwl_config::serialDebugSummerbypass == 1) {
      Serial.println(F("loopBypassSummerSetFlaps"));
      Serial.print(F("bypassFlapSetpoint: "));
      Serial.println(bypassFlapSetpoint);
      Serial.print(F("bypassFlapState: "));
      Serial.println(bypassFlapState);
      Serial.print(F("bypassFlapStateDriveRunning: "));
      Serial.println(bypassFlapStateDriveRunning);
    }
    previousMillisBypassSummerSetFlaps = currentMillis;
    if (bypassFlapSetpoint != bypassFlapState) {    // bypassFlapState wird NACH erfolgter Fahrt gesetzt
      if ((bypassFlapsStartTime == 0)  || (millis() - bypassFlapsStartTime > bypassFlapsDriveTime)) {
        if (!bypassFlapsRunning) {
          if (kwl_config::serialDebugSummerbypass == 1) {
            Serial.println(F("Jetzt werden die Relais angesteuert"));
          }
          // Jetzt werden die Relais angesteuert
          if (bypassFlapSetpoint == bypassFlapState_Close) {

            // Erst Richtung, dann Power
            relBypassDirection.off();
            relBypassPower.on();

            bypassFlapsRunning = true;
            bypassFlapStateDriveRunning = bypassFlapState_Close;
            bypassFlapsStartTime = millis();
          } else if (bypassFlapSetpoint == bypassFlapState_Open) {

            // Erst Richtung, dann Power
            relBypassDirection.on();
            relBypassPower.on();

            bypassFlapsRunning = true;
            bypassFlapStateDriveRunning = bypassFlapState_Open;
            bypassFlapsStartTime = millis();
          }
        } else {
          if (kwl_config::serialDebugSummerbypass == 1) {
            Serial.println(F("Klappe wurde gefahren, jetzt abschalten"));
          }
          // Klappe wurde gefahren, jetzt abschalten
          // Relais ausschalten
          // Erst Power, dann Richtung beim Ausschalten
          relBypassDirection.off();
          relBypassPower.off();

          bypassFlapsRunning = false;
          bypassFlapState = bypassFlapStateDriveRunning;
          bypassLastChangeMillis = millis();
          // MQTT senden
          mqttCmdSendBypassState = true;
        }
        Serial.println(F("Nach der Schleife"));
        Serial.println(F("loopBypassSummerSetFlaps"));
        Serial.print(F("bypassFlapSetpoint: "));
        Serial.println(bypassFlapSetpoint);
        Serial.print(F("bypassFlapState: "));
        Serial.println(bypassFlapState);
        Serial.print(F("bypassFlapStateDriveRunning: "));
        Serial.println(bypassFlapStateDriveRunning);
      }
    }
  }
}

void loopTachoFan() {
  // Die Geschwindigkeit der beiden Lüfter wird bestimmt. Die eigentliche Zählung der Tachoimpulse
  // geschieht per Interrupt in countUpFan1 und countUpFan2
  currentMillis = millis();
  if (currentMillis - previousMillisFan >= intervalTachoFan) {

    noInterrupts();
    // Variablen umkopieren und resetten
    fan1speed.capture();
    fan2speed.capture();
    interrupts();

    speedTachoFan1 = fan1speed.get_speed();
    speedTachoFan2 = fan2speed.get_speed();
    if (kwl_config::serialDebugFan == 1) {
      Serial.print(F("Speed fan1: "));
      Serial.print(speedTachoFan1);
      Serial.print(F(", fan2: "));
      Serial.println(speedTachoFan2);
    }
    previousMillisFan = currentMillis;
  }
}

void loopSetFan() {
  currentMillis = millis();
  if (currentMillis - previousMillisSetFan > intervalSetFan) {
    //Serial.println ("loopSetFan");
    previousMillisSetFan = currentMillis;
    if (FanMode == FanMode_Normal) {
      setSpeedToFan();
    } else if (FanMode == FanMode_Calibration) {
      SpeedCalibrationPwm();
    }
  }
}

void loopEffiencyCalc() {
  // Berechnung des Wirkungsgrades
  currentMillis = millis();
  if ((currentMillis - previousMillisEffiencyCalc > intervalEffiencyCalc) || EffiencyCalcNow) {
    previousMillisEffiencyCalc = currentMillis;
    if (TEMP3_Abluft - TEMP1_Aussenluft != 0 ) {
      EffiencyKwl = (int) (100 * (TEMP2_Zuluft - TEMP1_Aussenluft) / (TEMP3_Abluft - TEMP1_Aussenluft));
    }
  }
}

void loopCheckForErrors() {
  // In dieser Funktion wird auf verschiedene Fehler getestet und der gravierenste Fehler wird in die Variable ErrorText geschrieben
  // ErrorText wird auf das Display geschrieben.
  currentMillis = millis();
  if (currentMillis - previousMillisCheckForErrors > intervalCheckForErrors) {
    previousMillisCheckForErrors = currentMillis;

    if (kwl_config::StandardKwlModeFactor[kwlMode] != 0 && speedTachoFan1 < 10 && !antifreezeState && speedTachoFan2 < 10 ) {
      ErrorText = "Beide Luefter ausgefallen";
      return;
    }
    else if (kwl_config::StandardKwlModeFactor[kwlMode] != 0 && speedTachoFan1 < 10 && !antifreezeState ) {
      ErrorText = "Zuluftluefter ausgefallen";
      return;
    }
    else if (kwl_config::StandardKwlModeFactor[kwlMode] != 0 && speedTachoFan2 < 10 ) {
      ErrorText = "Abluftluefter ausgefallen";
      return;
    }
    else if (TEMP1_Aussenluft == -127.0 || TEMP2_Zuluft == -127.0 || TEMP3_Abluft == -127.0 || TEMP4_Fortluft == -127.0) {
      ErrorText = "Temperatursensor(en) ausgefallen: ";
      if (TEMP1_Aussenluft == -127.0) ErrorText += "T1 ";
      if (TEMP2_Zuluft == -127.0) ErrorText += "T2 ";
      if (TEMP3_Abluft == -127.0) ErrorText += "T3 ";
      if (TEMP4_Fortluft == -127.0) ErrorText += "T4 ";
    }
    else
    {
      ErrorText = "";
    }

    // InfoText
    if (FanMode == FanMode_Calibration) {
      InfoText = "Luefter werden kalibriert fuer Stufe ";
      InfoText += actKwlMode;
      InfoText += ".   Bitte warten...";
    }
    else if (antifreezeState == antifreezePreheater) {
      InfoText = "Defroster: Vorheizregister eingeschaltet ";
      InfoText += int(techSetpointPreheater / 10);
      InfoText += "%";
    }
    else if (antifreezeState == antifreezeZuluftOff) {
      InfoText = "Defroster: Zuluftventilator ausgeschaltet!";
    }
    else if (antifreezeState == antifreezeFireplace) {
      InfoText = "Defroster: Zu- und Abluftventilator AUS! (KAMIN)";
    }
    else if (bypassFlapsRunning == true) {
      if (bypassFlapStateDriveRunning == bypassFlapState_Close) {
        InfoText = "Sommer-Bypassklappe wird geschlossen.";
      } else if (bypassFlapStateDriveRunning == bypassFlapState_Open) {
        InfoText = "Sommer-Bypassklappe wird geoeffnet.";
      }
    }
    else
    {
      InfoText = "";
    }
  }
}

/**********************************************************************
  Setup Routinen
**********************************************************************/

/****************************************
  Werte auslesen und Variablen zuweisen *
  **************************************/
void initializeVariables()
{
  Serial.println();
  Serial.println(F("initializeVariables"));
  int temp = 0;

  // Normdrehzahl Lüfter 1
  eeprom_read_int (2, &temp);
  StandardSpeedSetpointFan1 = temp;

  // Normdrehzahl Lüfter 2
  eeprom_read_int (4, &temp);
  StandardSpeedSetpointFan2 = temp;

  // bypassTempAbluftMin
  eeprom_read_int (6, &temp);
  bypassTempAbluftMin = temp;

  // bypassTempAussenluftMin
  eeprom_read_int (8, &temp);
  bypassTempAussenluftMin = temp;

  // bypassHystereseMinutes Close
  eeprom_read_int (10, &temp);
  bypassHystereseMinutes = temp;

  // antifreezeHyst
  eeprom_read_int (12, &temp);
  antifreezeHyst = temp;
  antifreezeTempUpperLimit = antifreezeTemp + antifreezeHyst;

  // bypassManualSetpoint Close
  eeprom_read_int (14, &temp);
  bypassManualSetpoint = temp;

  // bypassMode Auto
  eeprom_read_int (16, &temp);
  bypassMode = temp;

  // PWM für max 10 Lüftungsstufen und zwei Lüfter und einem Integer
  // max 10 Werte * 2 Lüfter * 2 Byte
  // 20 bis 60
  if (kwl_config::StandardModeCnt > 10) {
    Serial.println(F("ERROR: ModeCnt zu groß"));
  }
  for (int i = 0; ((i < kwl_config::StandardModeCnt) && (i < 10)); i++) {
    eeprom_read_int (20 + (i * 4), &temp);
    PwmSetpointFan1[i] = temp;
    eeprom_read_int (22 + (i * 4), &temp);
    PwmSetpointFan2[i] = temp;
  }
  // ENDE PWM für max 10 Lüftungsstufen

  // heatingAppCombUse
  eeprom_read_int (60, &temp);
  heatingAppCombUse = temp;

  // Weiter geht es ab Speicherplatz 62dez ff
}


void loopWrite100Millis() {
  currentMillis = millis();
  if (currentMillis - previous100Millis > 100) {
    previous100Millis = currentMillis;
    Serial.print ("Timestamp: ");
    Serial.println ((long)currentMillis);
  }
}

// *** SETUP START ***
void setup()
{
  Serial.begin(57600); // Serielle Ausgabe starten

  // *** TFT AUSGABE ***
  SetupTftScreen();
  SetupTouch();
  print_header();

  initializeEEPROM();
  initializeVariables();

  Serial.println();
  SetCursor(0, 30);
  initTracer.println(F("Booting... "));

  if (heatingAppCombUse == 1) {
    initTracer.println(F("...System mit Feuerstaettenbetrieb"));
  }

  initTracer.print(F("Initialisierung Ethernet:"));
  uint8_t mac[6];
  kwl_config::mac.copy_to(mac);
  Ethernet.begin(mac, kwl_config::ip, kwl_config::DnServer, kwl_config::gateway, kwl_config::subnet);
  delay(1500);    // Delay in Setup erlaubt
  lastMqttReconnectAttempt = 0;
  lastLanReconnectAttempt = 0;
  initTracer.print(F(" IP Adresse: "));
  initTracer.println(Ethernet.localIP());
  bLanOk = true;

  if (Ethernet.localIP()[0] == 0) {
    initTracer.println();
    initTracer.println(F("...FEHLER: KEINE LAN VERBINDUNG, WARTEN 15 Sek."));
    bLanOk = false;
    delay(15000);
    // 30 Sekunden Pause
  }

  initTracer.println(F("Initialisierung Mqtt"));
  mqttClient.setServer(kwl_config::mqttbroker, 1883);
  mqttClient.setCallback(mqttReceiveMsg);

  Serial.println(F("Initialisierung Temperatursensoren"));
  tft.println   (F("Initialisierung Temperatursensoren"));
  // Temperatursensoren
  Temp1Sensor.begin();
  Temp1Sensor.setResolution(TEMPERATURE_PRECISION);
  Temp1Sensor.setWaitForConversion(false);
  Temp2Sensor.begin();
  Temp2Sensor.setResolution(TEMPERATURE_PRECISION);
  Temp2Sensor.setWaitForConversion(false);
  Temp3Sensor.begin();
  Temp3Sensor.setResolution(TEMPERATURE_PRECISION);
  Temp3Sensor.setWaitForConversion(false);
  Temp4Sensor.begin();
  Temp4Sensor.setResolution(TEMPERATURE_PRECISION);
  Temp4Sensor.setWaitForConversion(false);

  Serial.println(F("Initialisierung Ventilatoren"));
  tft.println(F("Initialisierung Ventilatoren"));
  // Lüfter Speed
  pinMode(kwl_config::PinFan1PWM, OUTPUT);
  digitalWrite(kwl_config::PinFan1PWM, LOW);
  pinMode(kwl_config::PinFan2PWM, OUTPUT);
  digitalWrite(kwl_config::PinFan2PWM, LOW);

  // Lüfter Tacho Interrupt
  pinMode(kwl_config::PinFan1Tacho, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(kwl_config::PinFan1Tacho), countUpFan1, RISING );

  Serial.print (F("Pin und Interrupt: "));
  Serial.print (kwl_config::PinFan1Tacho);
  Serial.print (F("\t"));
  Serial.println (digitalPinToInterrupt(kwl_config::PinFan1Tacho));

  pinMode(kwl_config::PinFan2Tacho, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(kwl_config::PinFan2Tacho), countUpFan2, RISING );

  Serial.print (F("Pin und Interrupt: "));
  Serial.print (kwl_config::PinFan2Tacho);
  Serial.print (F("\t"));
  Serial.println (digitalPinToInterrupt(kwl_config::PinFan2Tacho));

  // Relais Ansteuerung Lüfter
  relFan1Power.on();
  relFan2Power.on();

  // Relais Ansteuerung Bypass
  relBypassPower.off();
  relBypassDirection.off();

  if (ControlFansDAC == 1) {
    Wire.begin();               // I2C-Pins definieren
    Serial.println(F("Initialisierung DAC"));
    tft.println   (F("Initialisierung DAC"));
  }

  // DHT Sensoren
  dht1.begin();
  dht2.begin();
  delay(1500);
  initTracer.println(F("Initialisierung Sensoren"));
  sensors_event_t event;
  dht1.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    DHT1IsAvailable = true;
    initTracer.println(F("...gefunden: DHT1"));
  } else {
    initTracer.println(F("...NICHT gefunden: DHT1"));
  }
  dht2.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    DHT2IsAvailable = true;
    initTracer.println(F("...gefunden: DHT2"));
  } else {
    initTracer.println(F("...NICHT gefunden: DHT2"));
  }

  // MH-Z14 CO2 Sensor
  if (SetupMHZ14()) {
    initTracer.println(F("...gefunden: CO2 Sensor MH-Z14"));
  } else {
    initTracer.println(F("...NICHT gefunden: CO2 Sensor MH-Z14"));
  }

  // TGS2600 VOC Sensor
  if (SetupTGS2600()) {
    initTracer.println(F("...gefunden: VOC Sensor TGS2600"));
  } else {
    initTracer.println(F("...NICHT gefunden: VOC Sensor TGS2600"));
  }

  // Setup fertig
  initTracer.println(F("Setup completed..."));

  // 4 Sekunden Pause für die TFT Anzeige, damit man sie auch lesen kann
  delay (4000);

  SetupBackgroundScreen();   // Bootmeldungen löschen, Hintergrund für Standardanzeige starten

  //PID
  //turn the PID on
  // ab hier keine delays mehr verwenden, der Zeitnehmer für die PID-Regler laufen
  PidFan1.SetOutputLimits(0, 1000);
  PidFan1.SetMode(AUTOMATIC);
  PidFan1.SetSampleTime(intervalSetFan);

  PidFan2.SetOutputLimits(0, 1000);
  PidFan2.SetMode(AUTOMATIC);
  PidFan2.SetSampleTime(intervalSetFan);

  PidPreheater.SetOutputLimits(100, 1000);
  PidPreheater.SetMode(MANUAL);
  PidPreheater.SetSampleTime(intervalSetFan);  // SetFan ruft Preheater auf, deswegen hier intervalSetFan

  previousMillisTemp = millis();

}
// *** SETUP ENDE

// *** LOOP START ***
void loop()
{
  //loopWrite100Millis();

  loopTachoFan();
  loopSetFan();
  loopAntiFreezeCheck();
  loopBypassSummerCheck();
  loopBypassSummerSetFlaps();
  loopTemperaturRequest();
  loopTemperaturRead();
  loopDHTRead();
  loopMHZ14Read();
  loopVocRead();
  loopEffiencyCalc();
  loopCheckForErrors();

  loopMqttSendMode();
  loopMqttSendFan();
  loopMqttSendTemp();
  loopMqttSendDHT();
  loopMqttSendBypass();

  loopDisplayUpdate();

  loopMqttHeartbeat();
  loopNetworkConnection();
  loopTouch();
}
// *** LOOP ENDE ***


