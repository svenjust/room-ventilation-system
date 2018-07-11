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
#include <Wire.h>
#include <DHT.h>
#include <DHT_U.h>

#include "MultiPrint.h"
#include "NetworkClient.h"
#include "Relay.h"
#include "Task.h"
#include "TempSensors.h"
#include "FanControl.h"
#include "Antifreeze.h"
#include "ProgramManager.h"
#include "SummerBypass.h"
#include "KWLPersistentConfig.h"

#include "KWLConfig.h"
#include "MQTTTopic.hpp"
#include "MicroNTP.h"
#include "EthernetUdp.h"


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


// Sind die folgenden Variablen auf true, werden beim nächsten Durchlauf die entsprechenden mqtt Messages gesendet,
// anschliessend wird die Variable wieder auf false gesetzt
boolean mqttCmdSendDht                         = false;
boolean mqttCmdSendMHZ14                       = false;
boolean mqttCmdSendMode                        = false;
boolean mqttCmdSendConfig                      = false;

char   TEMPChar[10];            // Hilfsvariable zu Konvertierung
char   buffer[7];               // the ASCII of the integer will be stored in this char array
String TEMPAsString;            // Ausgelesene Wert als String
String ErrorText;               // Textvariable für Fehlermeldung
String InfoText;                // Textvariable für Infomeldung


// Definitionen für das Scheduling
unsigned long intervalCheckForErrors         = 1000;
unsigned long intervalDHTRead                = 10000;
unsigned long intervalMHZ14Read              = 10000;
unsigned long intervalTGS2600Read            = 30000;

unsigned long intervalMqttTemp               = 5000;
unsigned long intervalMqttMHZ14              = 60000;
unsigned long intervalMqttTempOversampling   = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long intervalMqttMHZ14Oversampling  = 300000; // 5 * 60 * 1000; 5 Minuten
unsigned long previousMillisCheckForErrors        = 0;
unsigned long previousMillisDHTRead               = 0;
unsigned long previousMillisMHZ14Read             = 0;
unsigned long previousMillisTGS2600Read           = 0;

unsigned long previousMillisMqttDht               = 0;
unsigned long previousMillisMqttDhtOversampling   = 0;
unsigned long previousMillisMqttMHZ14             = 0;
unsigned long previousMillisMqttMHZ14Oversampling = 0;

unsigned long previous100Millis                   = 0;
unsigned long previousMillisTemp                  = 0;
unsigned long currentMillis                       = 0;


/// Class comprising all modules for the control of the ventilation system.
class KWLControl : private FanControl::SetSpeedCallback, private MessageHandler
{
public:
  KWLControl() :
    fan_control_(persistent_config_, this),
    bypass_(persistent_config_, temp_sensors_),
    antifreeze_(fan_control_, temp_sensors_, persistent_config_),
    ntp_(udp_, KWLConfig::NetworkNTPServer),
    program_manager_(persistent_config_, fan_control_, ntp_)
  {}

  /// Start the controller.
  void begin(Print& initTracer) {
    persistent_config_.begin(initTracer, KWLConfig::FACTORY_RESET_EEPROM);
    network_client_.begin(initTracer);
    temp_sensors_.begin(initTracer);
    fan_control_.begin(initTracer);
    bypass_.begin(initTracer);
    antifreeze_.begin(initTracer);
    ntp_.begin();
    program_manager_.begin();
  }

  /// Get persistent configuration.
  KWLPersistentConfig& getPersistentConfig() { return persistent_config_; }

  /// Get network client.
  NetworkClient& getNetworkClient() { return network_client_; }

  /// Get temperature sensor array.
  TempSensors& getTempSensors() { return temp_sensors_; }

  /// Get fan controlling object.
  FanControl& getFanControl() { return fan_control_; }

  /// Get bypass controlling object.
  SummerBypass& getBypass() { return bypass_; }

  /// Get antifreeze control.
  Antifreeze& getAntifreeze() { return antifreeze_; }

  /// Get NTP client.
  MicroNTP& getNTP() { return ntp_; }

private:
  virtual void fanSpeedSet() override {
    // this callback is called after computing new PWM tech points
    // and before setting fan speed via PWM
    antifreeze_.doActionAntiFreezeState();
  }

  virtual bool mqttReceiveMsg(const StringView& topic, const StringView& s) override
  {
    // Set Values
    if (topic == MQTTTopic::CmdResetAll) {
      if (s == F("YES"))   {
        Serial.println(F("Speicherbereich wird gelöscht"));
        getPersistentConfig().factoryReset();
        // Reboot
        Serial.println(F("Reboot"));
        delay (1000);
        asm volatile ("jmp 0");
      }
    } else if (topic == MQTTTopic::KwlDebugsetSchedulerResetvalues) {
      // reset maximum runtimes for all tasks
      Task::resetAllMaxima();
    // Get Commands
    } else if (topic == MQTTTopic::CmdGetvalues) {
      // Alle Values
      getTempSensors().forceSend();
      getAntifreeze().forceSend();
      getFanControl().forceSendSpeed();
      getFanControl().forceSendMode();
      getBypass().forceSend();
      mqttCmdSendDht             = true;
      mqttCmdSendMHZ14           = true;
    } else if (topic == MQTTTopic::KwlDebugsetSchedulerGetvalues) {
      // send statistics for scheduler
      char buffer[150];
      for (auto i = Task::begin(); i != Task::end(); ++i) {
        i->getStatistics().toString(buffer, sizeof(buffer));
        publish(MQTTTopic::KwlDebugstateScheduler, buffer, false);
      }
      Task::getSchedulerStatistics().toString(buffer, sizeof(buffer));
      publish(MQTTTopic::KwlDebugstateScheduler, buffer, false);
      Task::getAllTasksStatistics().toString(buffer, sizeof(buffer));
      publish(MQTTTopic::KwlDebugstateScheduler, buffer, false);
    } else {
      return false;
    }
    return true;
  }

  /// Persistent configuration.
  KWLPersistentConfig persistent_config_;
  /// Global MQTT client.
  NetworkClient network_client_;
  /// Set of temperature sensors
  TempSensors temp_sensors_;
  /// Fan control.
  FanControl fan_control_;
  /// Summer bypass object.
  SummerBypass bypass_;
  /// Antifreeze/preheater control.
  Antifreeze antifreeze_;
  /// UDP handler for NTP.
  EthernetUDP udp_;
  /// NTP protocol.
  MicroNTP ntp_;  // will be moved to timed program handling routines and made a task
  /// Program manager to set daily/weekly programs.
  ProgramManager program_manager_;
};

KWLControl kwlControl;

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

void loopCheckForErrors() {
  // In dieser Funktion wird auf verschiedene Fehler getestet und der gravierenste Fehler wird in die Variable ErrorText geschrieben
  // ErrorText wird auf das Display geschrieben.
  currentMillis = millis();
  if (currentMillis - previousMillisCheckForErrors > intervalCheckForErrors) {
    previousMillisCheckForErrors = currentMillis;

    if (KWLConfig::StandardKwlModeFactor[kwlControl.getFanControl().getVentilationMode()] != 0
        && kwlControl.getFanControl().getFan1().getSpeed() < 10
        && kwlControl.getAntifreeze().getState() == AntifreezeState::OFF
        && kwlControl.getFanControl().getFan2().getSpeed() < 10 ) {
      ErrorText = "Beide Luefter ausgefallen";
      return;
    }
    else if (KWLConfig::StandardKwlModeFactor[kwlControl.getFanControl().getVentilationMode()] != 0
             && kwlControl.getFanControl().getFan1().getSpeed() < 10
             && kwlControl.getAntifreeze().getState() == AntifreezeState::OFF ) {
      ErrorText = "Zuluftluefter ausgefallen";
      return;
    }
    else if (KWLConfig::StandardKwlModeFactor[kwlControl.getFanControl().getVentilationMode()] != 0
             && kwlControl.getFanControl().getFan2().getSpeed() < 10 ) {
      ErrorText = "Abluftluefter ausgefallen";
      return;
    }
    else if (kwlControl.getTempSensors().get_t1_outside() == TempSensors::INVALID
             || kwlControl.getTempSensors().get_t2_inlet() == TempSensors::INVALID
             || kwlControl.getTempSensors().get_t3_outlet() == TempSensors::INVALID
             || kwlControl.getTempSensors().get_t4_exhaust() == TempSensors::INVALID) {
      ErrorText = "Temperatursensor(en) ausgefallen: ";
      if (kwlControl.getTempSensors().get_t1_outside() == TempSensors::INVALID) ErrorText += "T1 ";
      if (kwlControl.getTempSensors().get_t2_inlet() == TempSensors::INVALID) ErrorText += "T2 ";
      if (kwlControl.getTempSensors().get_t3_outlet() == TempSensors::INVALID) ErrorText += "T3 ";
      if (kwlControl.getTempSensors().get_t4_exhaust() == TempSensors::INVALID) ErrorText += "T4 ";
    }
    else
    {
      ErrorText = "";
    }

    // InfoText
    if (kwlControl.getFanControl().getMode() == FanMode::Calibration) {
      InfoText = "Luefter werden kalibriert fuer Stufe ";
      InfoText += kwlControl.getFanControl().getVentilationCalibrationMode();
      InfoText += ".   Bitte warten...";
    }
    else if (kwlControl.getAntifreeze().getState() == AntifreezeState::PREHEATER) {
      InfoText = "Defroster: Vorheizregister eingeschaltet ";
      InfoText += int(kwlControl.getAntifreeze().getPreheaterState());
      InfoText += "%";
    }
    else if (kwlControl.getAntifreeze().getState() == AntifreezeState::FAN_OFF) {
      InfoText = "Defroster: Zuluftventilator ausgeschaltet!";
    }
    else if (kwlControl.getAntifreeze().getState() == AntifreezeState::FIREPLACE) {
      InfoText = "Defroster: Zu- und Abluftventilator AUS! (KAMIN)";
    }
    else if (kwlControl.getBypass().isRunning() == true) {
      if (kwlControl.getBypass().getTargetState() == SummerBypassFlapState::CLOSED) {
        InfoText = "Sommer-Bypassklappe wird geschlossen.";
      } else if (kwlControl.getBypass().getTargetState() == SummerBypassFlapState::OPEN) {
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

void loopWrite100Millis() {
  currentMillis = millis();
  if (currentMillis - previous100Millis > 100) {
    previous100Millis = currentMillis;
    Serial.print (F("Timestamp: "));
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

  // Init tracer which prints to both TFT and Serial.Í
  static MultiPrint initTracer(Serial, tft);

  if (KWLConfig::ControlFansDAC) {
    // TODO Also if using Preheater DAC, but no Fan DAC
    Wire.begin();               // I2C-Pins definieren
    initTracer.println(F("Initialisierung DAC"));
  }

  kwlControl.begin(initTracer);

  Serial.println();
  SetCursor(0, 30);
  initTracer.println(F("Booting... "));

  if (kwlControl.getAntifreeze().getHeatingAppCombUse()) {
    initTracer.println(F("...System mit Feuerstaettenbetrieb"));
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

  previousMillisTemp = millis();
}
// *** SETUP ENDE

// *** LOOP START ***
void loop()
{
  Task::loop();
  //loopWrite100Millis();

  loopDHTRead();
  loopMHZ14Read();
  loopVocRead();
  loopCheckForErrors();

  loopMqttSendDHT();

  loopDisplayUpdate();
  loopTouch();

  kwlControl.getNTP().loop();
}
// *** LOOP ENDE ***


