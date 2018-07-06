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
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>       // mqtt Client
#include <PID_v1.h>             // PID-Regler für die Drehzahlregelung
#include <Wire.h>
#include <DHT.h>
#include <DHT_U.h>

#include "MultiPrint.h"
#include "MQTTClient.h"
#include "Relay.h"
#include "Scheduler.h"
#include "TempSensors.h"
#include "FanControl.h"
#include "KWLPersistentConfig.h"

#include "KWLConfig.h"
#include "MQTTTopic.hpp"


// ***************************************************  V E R S I O N S N U M M E R   D E R    S W   *************************************************
#define strVersion "v0.15"


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


Relay relBypassPower(KWLConfig::PinBypassPower);
Relay relBypassDirection(KWLConfig::PinBypassDirection);

// Sind die folgenden Variablen auf true, werden beim nächsten Durchlauf die entsprechenden mqtt Messages gesendet,
// anschliessend wird die Variable wieder auf false gesetzt
boolean mqttCmdSendDht                         = false;
boolean mqttCmdSendMHZ14                       = false;
boolean mqttCmdSendFans                        = false;
boolean mqttCmdSendBypassState                 = false;
boolean mqttCmdSendBypassAllValues             = false;
boolean mqttCmdSendMode                        = false;
boolean mqttCmdSendConfig                      = false;
// mqttDebug Messages
boolean mqttCmdSendAlwaysDebugPreheater        = false;

char   TEMPChar[10];            // Hilfsvariable zu Konvertierung
char   buffer[7];               // the ASCII of the integer will be stored in this char array
String TEMPAsString;            // Ausgelesene Wert als String
String ErrorText;               // Textvariable für Fehlermeldung
String InfoText;                // Textvariable für Infomeldung

// Start Definition für Heating Appliance (Feuerstätte)  //////////////////////////////////////////
int            heatingAppCombUse                    = KWLConfig::StandardHeatingAppCombUse;
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

int  bypassManualSetpoint        = KWLConfig::StandardBypassManualSetpoint;   // Standardstellung Bypass
int  bypassMode                  = KWLConfig::StandardBypassMode;             // Automatische oder Manuelle Steuerung der Bypass-Klappe
int  bypassFlapState             = bypassFlapState_Unknown;           // aktuelle Stellung der Bypass-Klappe
int  bypassFlapStateDriveRunning = bypassFlapState_Unknown;
int  bypassTempAbluftMin         = KWLConfig::StandardBypassTempAbluftMin;
int  bypassTempAussenluftMin     = KWLConfig::StandardBypassTempAussenluftMin;
int  bypassHystereseMinutes      = KWLConfig::StandardBypassHystereseMinutes;
int  bypassFlapSetpoint          = KWLConfig::StandardBypassManualSetpoint;

unsigned long bypassLastChangeMillis   = 0;                       // Letzte Änderung für Hysterese
long          bypassFlapsDriveTime = 120000; // 120 * 1000;       // Fahrzeit (ms) der Klappe zwischen den Stellungen Open und Close
boolean       bypassFlapsRunning = false;                         // True, wenn Klappe fährt
unsigned long bypassFlapsStartTime = 0;                           // Startzeit für den Beginn Klappenwechsels
// Ende  - Variablen für Bypass ///////////////////////////////////////////


// Definitionen für das Scheduling
unsigned long intervalAntifreezeCheck        = 60000;   //  60000 = 60 * 1000         // Frostschutzprüfung je Minute
unsigned long intervalAntiFreezeAlarmCheck   = 600000;  // 600000 = 10 * 60 * 1000;   // 10 Min Zeitraum zur Überprüfung, ob Vorheizregister die Temperatur erhöhen kann,
unsigned long intervalBypassSummerCheck      = 60000;   // Zeitraum zum Check der Bedingungen für BypassSummerCheck, 1 Minuten
unsigned long intervalBypassSummerSetFlaps   = 60000;   // 60000;  // 1 * 60 * 1000 Zeitraum zum Fahren des Bypasses, 1 Minuten
unsigned long intervalCheckForErrors         = 1000;
unsigned long intervalDHTRead                = 10000;
unsigned long intervalMHZ14Read              = 10000;
unsigned long intervalTGS2600Read            = 30000;

unsigned long intervalMqttTemp               = 5000;
unsigned long intervalMqttMHZ14              = 60000;
unsigned long intervalMqttTempOversampling   = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttMHZ14Oversampling  = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttBypassState        = 900000; //15 * 60 * 1000; 15 Minuten

unsigned long previousMillisFan                   = 0;
unsigned long previousMillisSetFan                = 0;
unsigned long previousMillisAntifreeze            = 0;
unsigned long previousMillisBypassSummerCheck     = 0;
unsigned long previousMillisBypassSummerSetFlaps  = 0;
unsigned long previousMillisCheckForErrors        = 0;
unsigned long previousMillisDHTRead               = 0;
unsigned long previousMillisMHZ14Read             = 0;
unsigned long previousMillisTGS2600Read           = 0;

unsigned long previousMillisMqttHeartbeat         = 0;
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


// forwards:
void DoActionAntiFreezeState();
void SetPreheater();

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
boolean bLanOk     = false;
boolean bMqttOk    = false;

/// Init tracer which prints to both TFT and Serial.
static MultiPrint initTracer(Serial, tft);
/// Global MQTT client.
// TODO move all MQTT service handling into the client
static MQTTClient mqttClientWrapper(mqttClient);
/// Persistent configuration.
static KWLPersistentConfig persistentConfig(initTracer, KWLConfig::FACTORY_RESET_EEPROM);
/// Task scheduler
static Scheduler scheduler;
/// Set of temperature sensors
static TempSensors tempSensors(scheduler, initTracer);
/// Fan control.
static FanControl fanControl(scheduler, persistentConfig,
    []() {
      // this callback is called after computing new PWM tech points
      // and before setting fan speed via PWM
      DoActionAntiFreezeState();
      SetPreheater();
    }, initTracer);

unsigned long lastMqttReconnectAttempt    = 0;
unsigned long lastLanReconnectAttempt = 0;

// PID REGLER
double heaterKp = 50, heaterKi = 0.1, heaterKd = 0.025;

//Specify the links and initial tuning parameters
PID PidPreheater(&tempSensors.get_t4_exhaust(), &techSetpointPreheater, &antifreezeTempUpperLimit, heaterKp, heaterKi, heaterKd, P_ON_M, DIRECT);
///////////////////////

//
float SendMqttDHT1Temp = 0;    // Temperatur Außenluft, gesendet per Mqtt
float SendMqttDHT1Hum  = 0;    // Temperatur Zuluft
float SendMqttDHT2Temp = 0;    // Temperatur Abluft
float SendMqttDHT2Hum  = 0;    // Temperatur Fortluft

int   SendMqttMHZ14CO2 = 0;    // CO2 Wert


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
      fanControl.setCalculateSpeedMode(FanCalculateSpeedMode::PROP);
    }
    if (s == "PID") {
      fanControl.setCalculateSpeedMode(FanCalculateSpeedMode::PID);
    }
  }
  else if (topicStr == MQTTTopic::CmdCalibrateFans) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "YES")   {
      fanControl.speedCalibrationStart();
    }
  }
  else if (topicStr == MQTTTopic::CmdResetAll) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "YES")   {
      Serial.println(F("Speicherbereich wird gelöscht"));
      persistentConfig.factoryReset();
      // Reboot
      Serial.println(F("Reboot"));
      delay (1000);
      asm volatile ("jmp 0");
    }
  }
  else if (topicStr == MQTTTopic::CmdAntiFreezeHyst) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    antifreezeHyst = i;
    antifreezeTempUpperLimit = antifreezeTemp + antifreezeHyst;
    // AntiFreezeHysterese
    persistentConfig.setBypassHystereseTemp(antifreezeHyst);
  }
  else if (topicStr == MQTTTopic::CmdBypassHystereseMinutes) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    bypassHystereseMinutes = i;
    persistentConfig.setBypassHystereseMinutes(i);
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
      persistentConfig.setHeatingAppCombUse(i);
      tempSensors.forceSend();
    }
    if (s == "NO")   {
      Serial.println(F("Feuerstättenmodus wird DEAKTIVIERT und gespeichert"));
      int i = 0;
      heatingAppCombUse = i;
      persistentConfig.setHeatingAppCombUse(i);
      tempSensors.forceSend();
    }
  }

  // Get Commands
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
    tempSensors.forceSend();
    mqttCmdSendDht             = true;
    mqttCmdSendFans            = true;
    mqttCmdSendBypassAllValues = true;
    mqttCmdSendMHZ14           = true;
  }

  // Debug Messages, den folgenden Block in der produktiven Version auskommentieren
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


void SetPreheater() {
  // Das Vorheizregister wird durch ein PID geregelt
  // Die Stellgröße darf zwischen 0..10V liegen
  // Wenn der Zuluftventilator unter einer Schwelle des Tachosignals liegt, wird das Vorheizregister IMMER ausgeschaltet (SICHERHEIT)
  // Schwelle: 1000 U/min

  if (KWLConfig::serialDebugAntifreeze == 1) {
    Serial.println (F("SetPreheater start"));
  }

  // Sicherheitsabfrage
  if (fanControl.getFan1().getSpeed() < 600 || fanControl.getFan1().isOff()) {
    // Sicherheitsabschaltung Vorheizer unter 600 Umdrehungen Zuluftventilator
    techSetpointPreheater = 0;
  }
  if (mqttCmdSendAlwaysDebugPreheater) {
    mqtt_debug_Preheater();
  }
  // Setzen per PWM
  analogWrite(KWLConfig::PinPreheaterPWM, techSetpointPreheater / 4);

  // Setzen der Werte per DAC
  byte HBy;
  byte LBy;

  // Preheater DAC
  HBy = techSetpointPreheater / 256;        //HIGH-Byte berechnen
  LBy = techSetpointPreheater - HBy * 256;  //LOW-Byte berechnen
  Wire.beginTransmission(KWLConfig::DacI2COutAddr); // Start Übertragung zur ANALOG-OUT Karte
  Wire.write(KWLConfig::DacChannelPreheater);        // PREHEATER schreiben
  Wire.write(LBy);                          // LOW-Byte schreiben
  Wire.write(HBy);                          // HIGH-Byte schreiben
  Wire.endTransmission();                   // Ende
}

void DoActionAntiFreezeState() {
  // Funktion wird ausgeführt, um AntiFreeze (Frostschutz) zu erreichen.

  switch (antifreezeState) {

    case antifreezePreheater:
      PidPreheater.Compute();
      break;

    case antifreezeZuluftOff:
      // Zuluft aus
      if (KWLConfig::serialDebugAntifreeze == 1)
        Serial.println (F("fan1 = 0"));
      fanControl.getFan1().off();
      techSetpointPreheater = 0;
      break;

    case antifreezeFireplace:
      // Feuerstättenmodus
      // beide Lüfter aus
      if (KWLConfig::serialDebugAntifreeze == 1) {
        Serial.println (F("fan1 = 0"));
        Serial.println (F("fan2 = 0"));
      }
      fanControl.getFan1().off();
      fanControl.getFan2().off();
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
    if (KWLConfig::serialDebugAntifreeze == 1)  Serial.println (F("loopAntiFreezeCheck start"));
    previousMillisAntifreeze = currentMillis;

    // antifreezeState = aktueller Status der AntiFrostSchaltung
    // Es wird in jeden Status überprüft, ob die Bedingungen für einen Statuswechsel erfüllt sind
    // Wenn ja, werden die einmaligen Aktionen (Setzen von Variablen) hier ausgeführt.
    // siehe auch "/Docs/Programming/Zustandsänderung Antifreeze.jpg"
    // Die regelmäßigen Aktionen für jedem Status werden in DoActionAntiFreezeState ausgeführt.
    switch (antifreezeState) {

      case antifreezeOff:  // antifreezeState = 0
        if ((tempSensors.get_t4_exhaust() <= antifreezeTemp) && (tempSensors.get_t1_outside() < 0.0)
            && (tempSensors.get_t4_exhaust() > TempSensors::INVALID) && (tempSensors.get_t1_outside() > TempSensors::INVALID))         // Wenn Sensoren fehlen, ist der Wert INVALID
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

        if (tempSensors.get_t4_exhaust() > antifreezeTemp + antifreezeHyst) {
          // Neuer Status: antifreezeOff
          antifreezeState = antifreezeOff;
          PidPreheater.SetMode(MANUAL);
        }

        if ((currentMillis - PreheaterStartMillis > intervalAntiFreezeAlarmCheck) &&
            (tempSensors.get_t4_exhaust() <= antifreezeTemp) && (tempSensors.get_t1_outside() < 0.0)
            && (tempSensors.get_t4_exhaust() > TempSensors::INVALID) && (tempSensors.get_t1_outside() > TempSensors::INVALID)) {
          // 10 Minuten vergangen seit antifreezeState == antifreezePreheater und Temperatur immer noch unter antifreezeTemp
          if (KWLConfig::serialDebugAntifreeze == 1) Serial.println (F("10 Minuten vergangen seit antifreezeState == antifreezePreheater"));

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
          if (tempSensors.get_t4_exhaust() > antifreezeTemp + antifreezeHyst) {
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

        if (KWLConfig::serialDebugAntifreeze == 1) {
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
    if (KWLConfig::serialDebugSummerbypass == 1) {
      Serial.println(F("BypassSummerCheck"));
    }
    previousMillisBypassSummerCheck = currentMillis;
    // Auto oder Manual?
    if (bypassMode == bypassMode_Auto) {
      if (KWLConfig::serialDebugSummerbypass == 1) {
        Serial.println(F("bypassMode_Auto"));
      }
      // Automatic
      // Hysterese überprüfen
      if (currentMillis - bypassLastChangeMillis >= (bypassHystereseMinutes * 60L * 1000L)) {
        if (KWLConfig::serialDebugSummerbypass == 1) {
          Serial.println(F("Time to Check"));
          Serial.print(F("TEMP1_Aussenluft       : "));
          Serial.println(tempSensors.get_t1_outside());
          Serial.print(F("TEMP3_Abluft           : "));
          Serial.println(tempSensors.get_t3_outlet());
          Serial.print(F("bypassTempAbluftMin    : "));
          Serial.println(bypassTempAbluftMin);
          Serial.print(F("bypassTempAussenluftMin: "));
          Serial.println(bypassTempAussenluftMin);
        }
        if ((tempSensors.get_t1_outside() > TempSensors::INVALID) && (tempSensors.get_t3_outlet() > TempSensors::INVALID)) {
          if ((tempSensors.get_t1_outside()    < tempSensors.get_t3_outlet() - 2)
              && (tempSensors.get_t3_outlet()     > bypassTempAbluftMin)
              && (tempSensors.get_t1_outside() > bypassTempAussenluftMin)) {
            //ok, dann Klappe öffen
            if (bypassFlapSetpoint != bypassFlapState_Open) {
              if (KWLConfig::serialDebugSummerbypass == 1) {
                Serial.println(F("Klappe öffen"));
              }
              bypassFlapSetpoint = bypassFlapState_Open;
              bypassLastChangeMillis = millis();
            }
          } else {
            //ok, dann Klappe schliessen
            if (bypassFlapSetpoint != bypassFlapState_Close) {
              if (KWLConfig::serialDebugSummerbypass == 1) {
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
    if (KWLConfig::serialDebugSummerbypass == 1) {
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
          if (KWLConfig::serialDebugSummerbypass == 1) {
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
          if (KWLConfig::serialDebugSummerbypass == 1) {
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

void loopCheckForErrors() {
  // In dieser Funktion wird auf verschiedene Fehler getestet und der gravierenste Fehler wird in die Variable ErrorText geschrieben
  // ErrorText wird auf das Display geschrieben.
  currentMillis = millis();
  if (currentMillis - previousMillisCheckForErrors > intervalCheckForErrors) {
    previousMillisCheckForErrors = currentMillis;

    if (KWLConfig::StandardKwlModeFactor[fanControl.getVentilationMode()] != 0 && fanControl.getFan1().getSpeed() < 10 && !antifreezeState && fanControl.getFan2().getSpeed() < 10 ) {
      ErrorText = "Beide Luefter ausgefallen";
      return;
    }
    else if (KWLConfig::StandardKwlModeFactor[fanControl.getVentilationMode()] != 0 && fanControl.getFan1().getSpeed() < 10 && !antifreezeState ) {
      ErrorText = "Zuluftluefter ausgefallen";
      return;
    }
    else if (KWLConfig::StandardKwlModeFactor[fanControl.getVentilationMode()] != 0 && fanControl.getFan2().getSpeed() < 10 ) {
      ErrorText = "Abluftluefter ausgefallen";
      return;
    }
    else if (tempSensors.get_t1_outside() == TempSensors::INVALID || tempSensors.get_t2_inlet() == TempSensors::INVALID || tempSensors.get_t3_outlet() == TempSensors::INVALID || tempSensors.get_t4_exhaust() == TempSensors::INVALID) {
      ErrorText = "Temperatursensor(en) ausgefallen: ";
      if (tempSensors.get_t1_outside() == TempSensors::INVALID) ErrorText += "T1 ";
      if (tempSensors.get_t2_inlet() == TempSensors::INVALID) ErrorText += "T2 ";
      if (tempSensors.get_t3_outlet() == TempSensors::INVALID) ErrorText += "T3 ";
      if (tempSensors.get_t4_exhaust() == TempSensors::INVALID) ErrorText += "T4 ";
    }
    else
    {
      ErrorText = "";
    }

    // InfoText
    if (fanControl.getMode() == FanMode::Calibration) {
      InfoText = "Luefter werden kalibriert fuer Stufe ";
      InfoText += fanControl.getVentilationCalibrationMode();
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

  // bypass
  bypassTempAbluftMin = persistentConfig.getBypassTempAbluftMin();
  bypassTempAussenluftMin = persistentConfig.getBypassTempAussenluftMin();
  bypassHystereseMinutes = persistentConfig.getBypassHystereseMinutes();
  bypassManualSetpoint = persistentConfig.getBypassManualSetpoint();
  bypassMode = persistentConfig.getBypassMode();

  // antifreeze
  antifreezeHyst = persistentConfig.getBypassHystereseTemp(); // TODO variable name is wrong
  antifreezeTempUpperLimit = antifreezeTemp + antifreezeHyst;

  // heatingAppCombUse
  heatingAppCombUse = persistentConfig.getHeatingAppCombUse();

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

  initializeVariables();

  Serial.println();
  SetCursor(0, 30);
  initTracer.println(F("Booting... "));

  if (heatingAppCombUse == 1) {
    initTracer.println(F("...System mit Feuerstaettenbetrieb"));
  }

  initTracer.print(F("Initialisierung Ethernet:"));
  uint8_t mac[6];
  KWLConfig::mac.copy_to(mac);
  Ethernet.begin(mac, KWLConfig::ip, KWLConfig::DnServer, KWLConfig::gateway, KWLConfig::subnet);
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
  mqttClient.setServer(KWLConfig::mqttbroker, 1883);
  mqttClient.setCallback(mqttReceiveMsg);

  // Relais Ansteuerung Bypass
  relBypassPower.off();
  relBypassDirection.off();

  if (KWLConfig::ControlFansDAC) {
    Wire.begin();               // I2C-Pins definieren
    initTracer.println(F("Initialisierung DAC"));
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

  PidPreheater.SetOutputLimits(100, 1000);
  PidPreheater.SetMode(MANUAL);
  PidPreheater.SetSampleTime(1000 /* TODO use constant intervalSetFan */);  // SetFan ruft Preheater auf, deswegen hier intervalSetFan

  previousMillisTemp = millis();

}
// *** SETUP ENDE

// *** LOOP START ***
void loop()
{
  scheduler.loop();
  //loopWrite100Millis();

  loopAntiFreezeCheck();
  loopBypassSummerCheck();
  loopBypassSummerSetFlaps();
  loopDHTRead();
  loopMHZ14Read();
  loopVocRead();
  loopCheckForErrors();

  loopMqttSendDHT();
  loopMqttSendBypass();

  loopDisplayUpdate();

  loopMqttHeartbeat();
  loopNetworkConnection();
  loopTouch();
}
// *** LOOP ENDE ***


