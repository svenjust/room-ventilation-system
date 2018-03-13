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

// ***************************************************  V E R S I O N S N U M M E R   D E R    S W   *************************************************
#define strVersion "v0.15"


// ***************************************************  A N S T E U E R U N G   P W M oder D A C   ***************************************************
#define  ControlFansDAC         1 // 1 = zusätzliche Ansteuerung durch DAC über SDA und SLC (und PWM)
// ****************************************** E N D E   A N S T E U E R U N G   P W M oder D A C   ***************************************************


// ***************************************************  A N S C H L U S S E I N S T E L L U N G E N ***************************************************
// Die Anschlüsse können dem Schaltplan entnommen werden: https://github.com/svenjust/room-ventilation-system/blob/master/Docs/circuit%20diagram/KWL-Steuerung%20f%C3%BCr%20P300.pdf

#define pwmPinFan1              44  // Lüfter Zuluft
#define pwmPinFan2              46  // Lüfter Abluft
#define pwmPinPreheater         45  // Vorheizer
#define tachoPinFan1            18  // Eingang mit Interrupt, Zuordnung von Pin zu Interrupt geschieht im Code mit der Funktion digitalPinToInterrupt
#define tachoPinFan2            19  // Eingang mit Interrupt, Zuordnung von Pin zu Interrupt geschieht im Code mit der Funktion digitalPinToInterrupt
#define relPinBypassPower       40  // Bypass Strom an/aus. Das BypassPower steuert, ob Strom am Bypass geschaltet ist, BypassDirection bestimmt Öffnen oder Schliessen
#define relPinBypassDirection   41  // Bypass Richtung, Stromlos = Schliessen (Winterstellung), Strom = Öffnen (Sommerstellung)
#define relPinFan1Power         42  // Stromversorgung Lüfter 1
#define relPinFan2Power         43  // Stromversorgung Lüfter 2

#define pinDHT1                 28  // Pin vom 1. DHT
#define pinDHT2                 29  // Pin vom 2. DHT

#define TEMP1_ONE_WIRE_BUS      30  // Für jeder Temperatursensor gibt es einen Anschluss auf dem Board, Vorteil: Temperatursensoren können per Kabel definiert werden, nicht Software
#define TEMP2_ONE_WIRE_BUS      31
#define TEMP3_ONE_WIRE_BUS      32
#define TEMP4_ONE_WIRE_BUS      33

// I2C nutzt beim Arduino Mega Pin 20 u 21
#define DAC_I2C_OUT_ADDR  176 >> 1 // I2C-OUTPUT-Addresse für Horter DAC als 7 Bit, wird verwendet als Alternative zur PWM Ansteuerung der Lüfter und für Vorheizregister
#define DAC_CHANNEL_FAN1         0 // Kanal 1 des DAC für Zuluft
#define DAC_CHANNEL_FAN2         1 // Kanal 2 des DAC für Abluft
#define DAC_CHANNEL_PREHEATER    2 // Kanal 3 des DAC für Vorheizregister

#define sensPinVoc               A15 //Analog Pin für VOC Sensor
// Serial2 nutzt beim Arduino Mega Pin 16 u 17
#define SerialMHZ14              Serial2  // CO2 Sensor (Winsen MH-Z14) wird über die Zweite Serielle Schnittstelle (Serial2) angeschlossen
// *******************************************E N D E ***  A N S C H L U S S E I N S T E L L U N G E N ***************************************************


// ***************************************************  N E T Z W E R K E I N S T E L L U N G E N ********************************************************
// Hier die IP Adresse für diese Steuerung und den MQTT Broker definieren.
byte mac[] = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };  // MAC Adresse des Ethernet Shields
IPAddress    ip         (192, 168,  20, 201);          // IP Adresse für diese Gerät im eigenen Netz
IPAddress    subnet     (255, 255, 255,   0);          // Subnet
IPAddress    gateway    (192, 168,  20, 250);          // Gateway
IPAddress    DnServer   (  8,   8,   8,   8);          // DNS Server, hier Google
IPAddress    mqttbroker (192, 168,  20, 240);          // IP Adresse des MQTT Brokers
// *******************************************E N D E ***  N E T Z W E R K E I N S T E L L U N G E N *****************************************************


// ******************************************* W E R K S E I N S T E L L U N G E N *************************************************************************
// Definition der Werkeinstellungen.
// Es können bis zu 10 Lüftungsstufen definiert werden. Im Allgemeinen sollten 4 oder 6 Stufen ausreichen.
// Die Originalsteuerung stellt 3 Stufen zur Verfügung
//
// Eine Definition für 4 Stufen, ähnlich der Originalsteuerung wäre:
// Stufe 0 = 0%, Stufe 1 = 70%, Stufe 2 = 100%, Stufe 3 = 130%. Stufe 0 ist hier zusätzlich.
//
// Ein mögliche Definition für 6 Stufen wäre bspw.:
// Stufe 0 = 0%, Stufe 1 = 60%, Stufe 2 = 80%, Stufe 3 = 100%, Stufe 4 = 120%, Stufe 4 = 140%
//
// defStandardModeCnt definiert die Anzahl der Stufen
//
// FACTORY_RESET_EEPROM = true setzt alle Werte der Steuerung auf eine definierte Werte zurück. Dieses entspricht einem
// Zurücksetzen auf den Werkzustand. Ein Factory-Reset kann auch per mqtt Befehl erreicht werden:
//     mosquitto_pub -t d15/set/kwl/resetAll_IKNOWWHATIMDOING -m YES
//
#define  FACTORY_RESET_EEPROM false // true = Werte im nichtflüchtigen Speicherbereich LÖSCHEN, false nichts tun

#define  defStandardModeCnt 4
double   defStandardKwlModeFactor[defStandardModeCnt] = {0, 0.7, 1, 1.3};   // Speichert die Solldrehzahlen in Relation zur Standardlüftungsstufe
int      kwlMode                            = 2;                            // Standardlüftungsstufe
#define  defStandardSpeedSetpointFan1      1550              // sju: 1450   // Drehzahlen für Standardlüftungsstufe
#define  defStandardSpeedSetpointFan2      1550              // sju: 1100
#define  defStandardNenndrehzahlFan        3200                             // Nenndrehzahl Papst Lüfter lt Datenblatt 3200 U/min
#define  defStandardBypassTempAbluftMin      24                             // Mindestablufttemperatur für die Öffnung des Bypasses im Automatik Betrieb
#define  defStandardBypassTempAussenluftMin  13                             // Mindestaussenlufttemperatur für die Öffnung des Bypasses im Automatik Betrieb
#define  defStandardBypassHystereseMinutes   60                             // Hystereszeit für eine Umstellung des Bypasses im Automatik Betrieb
#define  defStandardBypassHystereseTemp       3                             // Hysteretemperatur für eine Umstellung des Bypasses im Automatik Betrieb
#define  defStandardBypassManualSetpoint      1                             // 1 = Close, Stellung der Bypassklappen im manuellen Betrieb
#define  defStandardBypassMode                0                             // 0 = Auto, Automatik oder manueller Betrieb der Bypassklappe. 
// Im Automatikbetrieb steuert diese Steuerung die Bypass-Klappe, im manuellen Betrieb wird die Bypass-Klappe durch mqtt-Kommandos gesteuert.
#define  defStandardHeatingAppCombUse         0                             // 0 = NO, 1 = YES
// **************************************E N D E *** W E R K S E I N S T E L L U N G E N **********************************************************************


// ************************************** A N S T E U E R U N G   D E R    R E L A I S ************************************************************************
// Für die Lüfter und den Sommer-Bypass können bis zu vier Relais verbaut sein.
// Ohne Sommer-Bypass kann die Schaltung auch ohne Relais betrieben werden.
// Da verschiedene Relais unterschiedlich geschaltet werden, kann hier die logische
// Schaltung definiert werden.
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH
// ************************************** E N D E   A N S T E U E R U N G   D E R    R E L A I S ***************************************************************


// ***************************************************  D E B U G E I N S T E L L U N G E N ********************************************************
#define serialDebug              0 // 1 = Allgemeine Debugausgaben auf der seriellen Schnittstelle aktiviert
#define serialDebugFan           0 // 1 = Debugausgaben für die Lüfter auf der seriellen Schnittstelle aktiviert
#define serialDebugAntifreeze    0 // 1 = Debugausgaben für die Antifreezeschaltung auf der seriellen Schnittstelle aktiviert
#define serialDebugSummerbypass  1 // 1 = Debugausgaben für die Summerbypassschaltung auf der seriellen Schnittstelle aktiviert
#define serialDebugDisplay       0 // 1 = Debugausgaben für die Displayanzeige
#define serialDebugSensor        0 // 1 = Debugausgaben für die Sensoren
// *******************************************E N D E ***  D E B U G E I N S T E L L U N G E N *****************************************************


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


// MQTT Topics für die Kommunikation zwischen dieser Steuerung und einem mqtt Broker
// Die Topics sind in https://github.com/svenjust/room-ventilation-system/blob/master/Docs/mqtt%20topics/mqtt_topics.ods erläutert.

const char *TOPICCommand                    = "d15/set/#";
const char *TOPICCommandDebug               = "d15/debugset/#";
const char *TOPICCmdResetAll                = "d15/set/kwl/resetAll_IKNOWWHATIMDOING";
const char *TOPICCmdCalibrateFans           = "d15/set/kwl/calibratefans";
const char *TOPICCmdFansCalculateSpeedMode  = "d15/set/kwl/fans/calculatespeed";
const char *TOPICCmdFan1Speed               = "d15/set/kwl/fan1/standardspeed";
const char *TOPICCmdFan2Speed               = "d15/set/kwl/fan2/standardspeed";
const char *TOPICCmdGetSpeed                = "d15/set/kwl/fans/getspeed";
const char *TOPICCmdGetTemp                 = "d15/set/kwl/temperatur/gettemp";
const char *TOPICCmdGetvalues               = "d15/set/kwl/getvalues";
const char *TOPICCmdMode                    = "d15/set/kwl/lueftungsstufe";
const char *TOPICCmdAntiFreezeHyst          = "d15/set/kwl/antifreeze/hysterese";
const char *TOPICCmdBypassGetValues         = "d15/set/kwl/summerbypass/getvalues";
const char *TOPICCmdBypassManualFlap        = "d15/set/kwl/summerbypass/flap";
const char *TOPICCmdBypassMode              = "d15/set/kwl/summerbypass/mode";
const char *TOPICCmdBypassHystereseMinutes  = "d15/set/kwl/summerbypass/HystereseMinutes";
const char *TOPICCmdHeatingAppCombUse       = "d15/set/kwl/heatingapp/combinedUse";

const char *TOPICHeartbeat                  = "d15/state/kwl/heartbeat";
const char *TOPICFan1Speed                  = "d15/state/kwl/fan1/speed";
const char *TOPICFan2Speed                  = "d15/state/kwl/fan2/speed";
const char *TOPICKwlOnline                  = "d15/state/kwl/heartbeat";
const char *TOPICStateKwlMode               = "d15/state/kwl/lueftungsstufe";
const char *TOPICKwlTemperaturAussenluft    = "d15/state/kwl/aussenluft/temperatur";
const char *TOPICKwlTemperaturZuluft        = "d15/state/kwl/zuluft/temperatur";
const char *TOPICKwlTemperaturAbluft        = "d15/state/kwl/abluft/temperatur";
const char *TOPICKwlTemperaturFortluft      = "d15/state/kwl/fortluft/temperatur";
const char *TOPICKwlEffiency                = "d15/state/kwl/effiencyKwl";
const char *TOPICKwlAntifreeze              = "d15/state/kwl/antifreeze";
const char *TOPICKwlBypassState             = "d15/state/kwl/summerbypass/flap";
const char *TOPICKwlBypassMode              = "d15/state/kwl/summerbypass/mode";
const char *TOPICKwlBypassTempAbluftMin     = "d15/state/kwl/summerbypass/TempAbluftMin";
const char *TOPICKwlBypassTempAussenluftMin = "d15/state/kwl/summerbypass/TempAussenluftMin";
const char *TOPICKwlBypassHystereseMinutes  = "d15/state/kwl/summerbypass/HystereseMinutes";
const char *TOPICKwlHeatingAppCombUse       = "d15/state/kwl/heatingapp/combinedUse";

const char *TOPICKwlDHT1Temperatur          = "d15/state/kwl/dht1/temperatur";
const char *TOPICKwlDHT2Temperatur          = "d15/state/kwl/dht2/temperatur";
const char *TOPICKwlDHT1Humidity            = "d15/state/kwl/dht1/humidity";
const char *TOPICKwlDHT2Humidity            = "d15/state/kwl/dht2/humidity";
const char *TOPICKwlCO2Abluft               = "d15/state/kwl/abluft/co2";
const char *TOPICKwlVOCAbluft               = "d15/state/kwl/abluft/voc";


// Die folgenden Topics sind nur für die SW-Entwicklung, und schalten Debugausgaben per mqtt ein und aus
const char *TOPICKwlDebugsetFan1Getvalues   = "d15/debugset/kwl/fan1/getvalues";
const char *TOPICKwlDebugsetFan2Getvalues   = "d15/debugset/kwl/fan2/getvalues";
const char *TOPICKwlDebugstateFan1          = "d15/debugstate/kwl/fan1";
const char *TOPICKwlDebugstateFan2          = "d15/debugstate/kwl/fan2";
const char *TOPICKwlDebugstatePreheater     = "d15/debugstate/kwl/preheater";

// Die folgenden Topics sind nur für die SW-Entwicklung, es werden Messwerte überschrieben, es kann damit der Sommer-Bypass und die Frostschutzschaltung getestet werden
const char *TOPICKwlDebugsetTemperaturAussenluft = "d15/debugset/kwl/aussenluft/temperatur";
const char *TOPICKwlDebugsetTemperaturZuluft     = "d15/debugset/kwl/zuluft/temperatur";
const char *TOPICKwlDebugsetTemperaturAbluft     = "d15/debugset/kwl/abluft/temperatur";
const char *TOPICKwlDebugsetTemperaturFortluft   = "d15/debugset/kwl/fortluft/temperatur";
// Ende Topics


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
volatile long tachoFan1TimeSum        = 0;
volatile long tachoFan2TimeSum        = 0;
volatile byte tachoFan1CountSum       = 0;
volatile byte tachoFan2CountSum       = 0;
unsigned long tachoFan1LastMillis     = 0;
unsigned long tachoFan2LastMillis     = 0;

int cycleFan1Counter                  = 0;
int cycleFan2Counter                  = 0;

double StandardSpeedSetpointFan1      = defStandardSpeedSetpointFan1;      // Solldrehzahl im U/min für Zuluft bei kwlMode = 2 (Standardlüftungsstufe), Drehzahlen werden aus EEPROM gelesen.
double StandardSpeedSetpointFan2      = defStandardSpeedSetpointFan2;      // Solldrehzahl im U/min für Abluft bei kwlMode = 2 (Standardlüftungsstufe)
double speedSetpointFan1              = 0;                                 // Solldrehzahl im U/min für Zuluft bei Berücksichtungs der Lüftungsstufe
double speedSetpointFan2              = 0;                                 // Solldrehzahl im U/min für Zuluft bei Berücksichtungs der Lüftungsstufe
double techSetpointFan1               = 0;                                 // PWM oder Analogsignal 0..1000 für Zuluft
double techSetpointFan2               = 0;                                 // PWM oder Analogsignal 0..1000 für Abluft
// Ende Variablen für Lüfter

int  PwmSetpointFan1[defStandardModeCnt];                                  // Speichert die pwm-Werte für die verschiedenen Drehzahlen
int  PwmSetpointFan2[defStandardModeCnt];

#define FanMode_Normal                 0
#define FanMode_Calibration            1
int FanMode                           = FanMode_Normal;                   // Umschaltung zum Kalibrieren der PWM Signale zur Erreichung der Lüfterdrehzahlen für jede Stufe
int actKwlMode = 0;

// Start Definition für Heating Appliance (Feuerstätte)  //////////////////////////////////////////
int            heatingAppCombUse                    = defStandardHeatingAppCombUse;
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

int  bypassManualSetpoint        = defStandardBypassManualSetpoint;   // Standardstellung Bypass
int  bypassMode                  = defStandardBypassMode;             // Automatische oder Manuelle Steuerung der Bypass-Klappe
int  bypassFlapState             = bypassFlapState_Unknown;           // aktuelle Stellung der Bypass-Klappe
int  bypassFlapStateDriveRunning = bypassFlapState_Unknown;
int  bypassTempAbluftMin         = defStandardBypassTempAbluftMin;
int  bypassTempAussenluftMin     = defStandardBypassTempAussenluftMin;
int  bypassHystereseMinutes      = defStandardBypassHystereseMinutes;
int  bypassFlapSetpoint          = defStandardBypassManualSetpoint;

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
OneWire Temp1OneWire(TEMP1_ONE_WIRE_BUS);     // Einrichten des OneWire Bus um die Daten der Temperaturfühler abzurufen
OneWire Temp2OneWire(TEMP2_ONE_WIRE_BUS);
OneWire Temp3OneWire(TEMP3_ONE_WIRE_BUS);
OneWire Temp4OneWire(TEMP4_ONE_WIRE_BUS);
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
DHT_Unified dht1(pinDHT1, DHT22);
DHT_Unified dht2(pinDHT2, DHT22);
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
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String topicStr = topic;

  // Set Values
  if (topicStr == TOPICCmdFansCalculateSpeedMode) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "PROP")  {
      FansCalculateSpeed = CalculateSpeed_PROP;
    }
    if (s == "PID") {
      FansCalculateSpeed = CalculateSpeed_PID;
    }
  }
  if (topicStr == TOPICCmdCalibrateFans) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "YES")   {
      Serial.println(F("Kalibrierung Lüfter wird gestartet"));
      SpeedCalibrationStart();
    }
  }
  if (topicStr == TOPICCmdResetAll) {
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
  if (topicStr == TOPICCmdFan1Speed) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    StandardSpeedSetpointFan1 = i;
    // Drehzahl Lüfter 1
    eeprom_write_int(2, i);
  }
  if (topicStr == TOPICCmdFan2Speed) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    StandardSpeedSetpointFan2 = i;
    // Drehzahl Lüfter 1
    eeprom_write_int(4, i);
  }
  if (topicStr == TOPICCmdMode) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    if (i <= defStandardModeCnt)  kwlMode = i;
    mqttCmdSendMode = true;
    // KWL Stufe
  }
  if (topicStr == TOPICCmdAntiFreezeHyst) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    antifreezeHyst = i;
    antifreezeTempUpperLimit = antifreezeTemp + antifreezeHyst;
    // AntiFreezeHysterese
    eeprom_write_int(12, i);
  }
  if (topicStr == TOPICCmdBypassHystereseMinutes) {
    payload[length] = '\0';
    String s = String((char*)payload);
    int i = s.toInt();
    bypassHystereseMinutes = i;
    // BypassHystereseMinutes
    eeprom_write_int(10, i);
  }
  if (topicStr == TOPICCmdBypassManualFlap) {
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
  if (topicStr == TOPICCmdBypassMode) {
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
  if (topicStr == TOPICCmdHeatingAppCombUse) {
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
  if (topicStr == TOPICCmdGetTemp) {
    payload[length] = '\0';
    String s = String((char*)payload);
    mqttCmdSendTemp = true;
  }
  if (topicStr == TOPICCmdGetSpeed) {
    payload[length] = '\0';
    String s = String((char*)payload);
    mqttCmdSendFans = true;
  }
  if (topicStr == TOPICCmdBypassGetValues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    mqttCmdSendBypassAllValues = true;
  }
  if (topicStr == TOPICCmdGetvalues) {
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
  if (topicStr == TOPICKwlDebugsetTemperaturAussenluft) {
    payload[length] = '\0';
    TEMP1_Aussenluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }
  if (topicStr == TOPICKwlDebugsetTemperaturZuluft) {
    payload[length] = '\0';
    TEMP2_Zuluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }
  if (topicStr == TOPICKwlDebugsetTemperaturAbluft) {
    payload[length] = '\0';
    TEMP3_Abluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }
  if (topicStr == TOPICKwlDebugsetTemperaturFortluft) {
    payload[length] = '\0';
    TEMP4_Fortluft = String((char*)payload).toFloat();
    EffiencyCalcNow = true;
    mqttCmdSendTemp = true;
  }

  if (topicStr == TOPICKwlDebugsetFan1Getvalues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "on")   {
      mqttCmdSendAlwaysDebugFan1 = true;
    }
    if (s == "off")  {
      mqttCmdSendAlwaysDebugFan1 = false;
    }
  }
  if (topicStr == TOPICKwlDebugsetFan2Getvalues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "on")   {
      mqttCmdSendAlwaysDebugFan2 = true;
    }
    if (s == "off")  {
      mqttCmdSendAlwaysDebugFan2 = false;
    }
  }
  // Debug Messages, bis hier auskommentieren

}

boolean mqttReconnect() {
  Serial.println (F("reconnect start"));
  Serial.println ((long)currentMillis);
  if (mqttClient.connect("arduinoClientKwl", TOPICHeartbeat, 0, true, "offline")) {
    // Once connected, publish an announcement...
    mqttClient.publish(TOPICHeartbeat, "online");
    // ... and resubscribe
    mqttClient.subscribe(TOPICCommand);
    mqttClient.subscribe(TOPICCommandDebug);
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

  speedSetpointFan1 = StandardSpeedSetpointFan1 * defStandardKwlModeFactor[kwlMode];
  speedSetpointFan2 = StandardSpeedSetpointFan2 * defStandardKwlModeFactor[kwlMode];

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
    techSetpointFan1 = PwmSetpointFan1[kwlMode] ;
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

  if (serialDebugFan == 1) {
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

    Serial.print (F("Fan 2: "));
    Serial.print (F("\tGap: "));
    Serial.print (speedTachoFan2 - speedSetpointFan2);
    Serial.print (F("\tspeedTachoFan2: "));
    Serial.print (speedTachoFan2);
    Serial.print (F("\ttechSetpointFan2: "));
    Serial.print (techSetpointFan2);
    Serial.print (F("\tspeedSetpointFan2: "));
    Serial.println(speedSetpointFan2);
  }
  // Setzen per PWM
  analogWrite(pwmPinFan1, techSetpointFan1 / 4);
  analogWrite(pwmPinFan2, techSetpointFan2 / 4);

  // Setzen der Werte per DAC
  if (ControlFansDAC == 1) {
    byte HBy;
    byte LBy;

    // FAN 1
    HBy = techSetpointFan1 / 256;        //HIGH-Byte berechnen
    LBy = techSetpointFan1 - HBy * 256;  //LOW-Byte berechnen
    Wire.beginTransmission(DAC_I2C_OUT_ADDR); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(DAC_CHANNEL_FAN1);             // FAN 1 schreiben
    Wire.write(LBy);                          // LOW-Byte schreiben
    Wire.write(HBy);                          // HIGH-Byte schreiben
    Wire.endTransmission();                   // Ende

    // FAN 2
    HBy = techSetpointFan2 / 256;        //HIGH-Byte berechnen
    LBy = techSetpointFan2 - HBy * 256;  //LOW-Byte berechnen
    Wire.beginTransmission(DAC_I2C_OUT_ADDR); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(DAC_CHANNEL_FAN2);             // FAN 2 schreiben
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

  if (serialDebugAntifreeze == 1) {
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
  analogWrite(pwmPinPreheater, techSetpointPreheater / 4);

  // Setzen der Werte per DAC
  byte HBy;
  byte LBy;

  // Preheater DAC
  HBy = techSetpointPreheater / 256;        //HIGH-Byte berechnen
  LBy = techSetpointPreheater - HBy * 256;  //LOW-Byte berechnen
  Wire.beginTransmission(DAC_I2C_OUT_ADDR); // Start Übertragung zur ANALOG-OUT Karte
  Wire.write(DAC_CHANNEL_PREHEATER);        // PREHEATER schreiben
  Wire.write(LBy);                          // LOW-Byte schreiben
  Wire.write(HBy);                          // HIGH-Byte schreiben
  Wire.endTransmission();                   // Ende
}

void countUpFan1() {
  cycleFan1Counter++;
  tachoFan1TimeSum += millis() - tachoFan1LastMillis;
  tachoFan1LastMillis = millis();
}
void countUpFan2() {
  cycleFan2Counter++;
  tachoFan2TimeSum += millis() - tachoFan2LastMillis;
  tachoFan2LastMillis = millis();
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
      if (serialDebugAntifreeze == 1) Serial.println (F("fan1 = 0"));
      techSetpointFan1 = 0;
      techSetpointPreheater = 0;
      break;

    case  antifreezeFireplace:
      // Feuerstättenmodus
      // beide Lüfter aus
      if (serialDebugAntifreeze == 1) {
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
    if (serialDebugAntifreeze == 1)  Serial.println (F("loopAntiFreezeCheck start"));
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
          if (serialDebugAntifreeze == 1) Serial.println (F("10 Minuten vergangen seit antifreezeState == antifreezePreheater"));

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

        if (serialDebugAntifreeze == 1) {
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
    if (serialDebugSummerbypass == 1) {
      Serial.println(F("BypassSummerCheck"));
    }
    previousMillisBypassSummerCheck = currentMillis;
    // Auto oder Manual?
    if (bypassMode == bypassMode_Auto) {
      if (serialDebugSummerbypass == 1) {
        Serial.println(F("bypassMode_Auto"));
      }
      // Automatic
      // Hysterese überprüfen
      if (currentMillis - bypassLastChangeMillis >= (bypassHystereseMinutes * 60L * 1000L)) {
        if (serialDebugSummerbypass == 1) {
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
              if (serialDebugSummerbypass == 1) {
                Serial.println(F("Klappe öffen"));
              }
              bypassFlapSetpoint = bypassFlapState_Open;
              bypassLastChangeMillis = millis();
            }
          } else {
            //ok, dann Klappe schliessen
            if (bypassFlapSetpoint != bypassFlapState_Close) {
              if (serialDebugSummerbypass == 1) {
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
    if (serialDebugSummerbypass == 1) {
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
          if (serialDebugSummerbypass == 1) {
            Serial.println(F("Jetzt werden die Relais angesteuert"));
          }
          // Jetzt werden die Relais angesteuert
          if (bypassFlapSetpoint == bypassFlapState_Close) {

            // Erst Richtung, dann Power
            digitalWrite(relPinBypassDirection, RELAY_OFF);
            digitalWrite(relPinBypassPower, RELAY_ON);

            bypassFlapsRunning = true;
            bypassFlapStateDriveRunning = bypassFlapState_Close;
            bypassFlapsStartTime = millis();
          } else if (bypassFlapSetpoint == bypassFlapState_Open) {

            // Erst Richtung, dann Power
            digitalWrite(relPinBypassDirection, RELAY_ON);
            digitalWrite(relPinBypassPower, RELAY_ON);

            bypassFlapsRunning = true;
            bypassFlapStateDriveRunning = bypassFlapState_Open;
            bypassFlapsStartTime = millis();
          }
        } else {
          if (serialDebugSummerbypass == 1) {
            Serial.println(F("Klappe wurde gefahren, jetzt abschalten"));
          }
          // Klappe wurde gefahren, jetzt abschalten
          // Relais ausschalten
          // Erst Power, dann Richtung beim Ausschalten
          digitalWrite(relPinBypassPower, RELAY_OFF);
          digitalWrite(relPinBypassDirection, RELAY_OFF);

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
  // Die Geschindigkeit der beiden Lüfter wird bestimmt. Die eigentliche Zählung der Tachoimpulse
  // geschieht per Interrupt in countUpFan1 und countUpFan2
  currentMillis = millis();
  if (currentMillis - previousMillisFan >= intervalTachoFan) {

    byte _cycleFan1Counter;
    byte _cycleFan2Counter;
    unsigned long _tachoFan1TimeSum;
    unsigned long _tachoFan2TimeSum;

    noInterrupts();
    // Variablen umkopieren
    _cycleFan1Counter = cycleFan1Counter;
    _cycleFan2Counter = cycleFan2Counter;
    _tachoFan1TimeSum = tachoFan1TimeSum;
    _tachoFan2TimeSum = tachoFan2TimeSum;
    cycleFan1Counter = 0;
    cycleFan2Counter = 0;
    tachoFan1TimeSum = 0;
    tachoFan2TimeSum = 0;
    interrupts();

    //Serial.println (_tachoFan1TimeSum);
    //Serial.println (_cycleFan1Counter);
    if (_tachoFan1TimeSum != 0) {
      speedTachoFan1 = _cycleFan1Counter * 60000 / _tachoFan1TimeSum;  // Umdrehungen pro Minute
    } else {
      speedTachoFan1 = 0;
    }
    if (_tachoFan2TimeSum != 0) {
      speedTachoFan2 = _cycleFan2Counter * 60000 / _tachoFan2TimeSum;  // Umdrehungen pro Minute
    } else {
      speedTachoFan2 = 0;
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

    if (defStandardKwlModeFactor[kwlMode] != 0 && speedTachoFan1 < 10 && !antifreezeState && speedTachoFan2 < 10 ) {
      ErrorText = "Beide Luefter ausgefallen";
      return;
    }
    else if (defStandardKwlModeFactor[kwlMode] != 0 && speedTachoFan1 < 10 && !antifreezeState ) {
      ErrorText = "Zuluftluefter ausgefallen";
      return;
    }
    else if (defStandardKwlModeFactor[kwlMode] != 0 && speedTachoFan2 < 10 ) {
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
  if (defStandardModeCnt > 10) {
    Serial.println(F("ERROR: ModeCnt zu groß"));
  }
  for (int i = 0; ((i < defStandardModeCnt) && (i < 10)); i++) {
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
  Serial.println(F("Booting... "));
  SetCursor(0, 30);
  tft.println   (F("Booting... "));

  if (heatingAppCombUse == 1) {
    Serial.println(F("...System mit Feuerstaettenbetrieb"));
    tft.println   (F("...System mit Feuerstaettenbetrieb"));
  }

  Serial.print(F("Initialisierung Ethernet:"));
  tft.print(F("Initialisierung Ethernet:"));
  Ethernet.begin(mac, ip, DnServer, gateway, subnet);
  delay(1500);    // Delay in Setup erlaubt
  lastMqttReconnectAttempt = 0;
  lastLanReconnectAttempt = 0;
  Serial.print(F(" IP Adresse: "));
  Serial.println(Ethernet.localIP());
  tft.print(F(" IP Adresse: "));
  tft.println(Ethernet.localIP());
  bLanOk = true;

  if (Ethernet.localIP()[0] == 0) {
    Serial.println();
    tft.println();
    Serial.println(F("...FEHLER: KEINE LAN VERBINDUNG, WARTEN 15 Sek."));
    tft.println   (F("...FEHLER: KEINE LAN VERBINDUNG, WARTEN 15 Sek."));
    bLanOk = false;
    delay(15000);
    // 30 Sekunden Pause
  }

  Serial.println(F("Initialisierung Mqtt"));
  tft.println   (F("Initialisierung Mqtt"));
  mqttClient.setServer(mqttbroker, 1883);
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
  pinMode(pwmPinFan1, OUTPUT);
  digitalWrite(pwmPinFan1, LOW);
  pinMode(pwmPinFan2, OUTPUT);
  digitalWrite(pwmPinFan2, LOW);

  // Lüfter Tacho Interrupt
  pinMode(tachoPinFan1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachoPinFan1), countUpFan1, FALLING );

  Serial.print (F("Pin und Interrupt: "));
  Serial.print (tachoPinFan1);
  Serial.print (F("\t"));
  Serial.println (digitalPinToInterrupt(tachoPinFan1));

  pinMode(tachoPinFan2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachoPinFan2), countUpFan2, FALLING );

  Serial.print (F("Pin und Interrupt: "));
  Serial.print (tachoPinFan2);
  Serial.print (F("\t"));
  Serial.println (digitalPinToInterrupt(tachoPinFan2));

  // Relais Ansteuerung Lüfter
  pinMode(relPinFan1Power, OUTPUT);
  digitalWrite(relPinFan1Power, RELAY_ON);
  pinMode(relPinFan2Power, OUTPUT);
  digitalWrite(relPinFan2Power, RELAY_ON);

  // Relais Ansteuerung Bypass
  pinMode(relPinBypassPower, OUTPUT);
  digitalWrite(relPinBypassPower, RELAY_OFF);
  pinMode(relPinBypassDirection, OUTPUT);
  digitalWrite(relPinBypassDirection, RELAY_OFF);

  if (ControlFansDAC == 1) {
    Wire.begin();               // I2C-Pins definieren
    Serial.println(F("Initialisierung DAC"));
    tft.println   (F("Initialisierung DAC"));
  }

  // DHT Sensoren
  dht1.begin();
  dht2.begin();
  delay(1500);
  Serial.println(F("Initialisierung Sensoren"));
  tft.println   (F("Initialisierung Sensoren"));
  sensors_event_t event;
  dht1.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    DHT1IsAvailable = true;
    Serial.println(F("...gefunden: DHT1"));
    tft.println   (F("...gefunden: DHT1"));
  } else {
    Serial.println(F("...NICHT gefunden: DHT1"));
    tft.println   (F("...NICHT gefunden: DHT1"));
  }
  dht2.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    DHT2IsAvailable = true;
    Serial.println(F("...gefunden: DHT2"));
    tft.println   (F("...gefunden: DHT2"));
  } else {
    Serial.println(F("...NICHT gefunden: DHT2"));
    tft.println   (F("...NICHT gefunden: DHT2"));
  }

  // MH-Z14 CO2 Sensor
  if (SetupMHZ14()) {
    Serial.println(F("...gefunden: CO2 Sensor MH-Z14"));
    tft.println   (F("...gefunden: CO2 Sensor MH-Z14"));
  } else {
    Serial.println(F("...NICHT gefunden: CO2 Sensor MH-Z14"));
    tft.println   (F("...NICHT gefunden: CO2 Sensor MH-Z14"));
  }

  // TGS2600 VOC Sensor
  if (SetupTGS2600()) {
    Serial.println(F("...gefunden: VOC Sensor TGS2600"));
    tft.println   (F("...gefunden: VOC Sensor TGS2600"));
  } else {
    Serial.println(F("...NICHT gefunden: VOC Sensor TGS2600"));
    tft.println   (F("...NICHT gefunden: VOC Sensor TGS2600"));
  }

  // Setup fertig
  Serial.println(F("Setup completed..."));
  tft.println   (F("Setup completed..."));

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


