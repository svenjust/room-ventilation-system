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
- ntpClient, nonblocking, auch im Fehlerfall
  Der Standard Arduino ntpClient blockiert im Fehlerfall für mindestens eine Sekunde
  https://github.com/blackketter/NTPClient
*/

// Tachosignal auslesen wie
// https://forum.arduino.cc/index.php?topic=145226.msg1102102#msg1102102

#include <Adafruit_GFX.h>       // TFT 
#include <MCUFRIEND_kbv.h>      // TFT
#include <EEPROM.h>             // Speicherung von Einstellungen
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>       // mqtt Client
#include <PID_v1.h>             // PID-Regler für die Drehzahlregelung
#include <OneWire.h>            // OneWire Temperatursensoren
#include <DallasTemperature.h>

// ntpClient, der folgende Client ist nonblocking, auch im Fehlerfall
// Der Standard Arduino ntpClient blockt im Fehlerfall für mindestens eine Sekunde
// https://github.com/blackketter/NTPClient
#include <NTPClient.h>


#define pwmPinFan1              44  // Lüfter Zuluft
#define pwmPinFan2              46  // Lüfter Abluft
#define pwmPinPreheater         45  // Vorheizer
#define tachoPinFan1             3  // Interrupt 1 Eingang ist beim Mega2560 Pin D20
#define tachoPinFan2             2  // Interrupt 0 Eingang ist beim Mega2560 Pin D21
// BypassPower steuert, ob Strom am Bypass geschaltet ist, BypassDirection bestimmt Öffnen oder Schliessen.
// Damit ist schaltungstechnisch sichergestellt, dass nicht gleichzeitig geöffnet und geschlossen wird.
// Dies ist die klassische Rolladenschaltung.
#define relPinBypassPower       40  // Bypass Strom an/aus. Das BypassPower steuert, ob Strom am Bypass geschaltet ist, BypassDirection bestimmt Öffnen oder Schliessen
#define relPinBypassDirection   41  // Bypass Richtung, Stromlos = Schliessen (Winterstellung), Strom = Öffnen (Sommerstellung)
#define relPinFan1Power         42  // Stromversorgung Lüfter 1
#define relPinFan2Power         43  // Stromversorgung Lüfter 2

// OneWire Sensoren, für jeder Temperatursensor gibt es einen Anschluss auf dem Board, Vorteil: Temperatursensoren können per Kabel definiert werden, nicht Software
// Data wire is plugged into pin 30-33 on the Arduino
#define TEMP1_ONE_WIRE_BUS 30
#define TEMP2_ONE_WIRE_BUS 31
#define TEMP3_ONE_WIRE_BUS 32
#define TEMP4_ONE_WIRE_BUS 33
#define TEMPERATURE_PRECISION TEMP_9_BIT     // Genauigkeit der Temperatursensoren 9_BIT, Standard sind 12_BIT

// Ansteuerung der Relais
// Für die Lüfter und den Sommer-Bypass können bis zu vier Relais verbaut sein.
// Ohne Sommer-Bypass kann die Schaltung auch ohne Relais betrieben werden.
// Das verschiedene Relais unterschiedlich geschaltet werden, kann hier die logische
// Schaltung definiert werden.
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// Update these with values suitable for your hardware/network.
byte mac[] = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };  // MAC Adresse des Ethernet Shields
IPAddress    ip(192, 168, 20, 201);           // IP Adresse für diese Gerät im eigenen Netz
IPAddress    mqttserver(192, 168, 20, 240);       // IP Adresse des MQTT Brokers

int serialDebug = 1;            // 1 = Allgemein Debugausgaben auf der seriellen Schnittstelle aktiviert
int serialDebugFan = 1;         // 1 = Debugausgaben für die Lüfter auf der seriellen Schnittstelle aktiviert
int serialDebugAntifreeze = 1;  // 1 = Debugausgaben für die Antifreezeschaltung auf der seriellen Schnittstelle aktiviert

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

// MQTT Topics für die Kommunikation zwischen dieser Steuerung und einem mqtt Broker
// Die Topics sind in https://github.com/svenjust/room-ventilation-system/blob/master/Docs/mqtt%20topics/mqtt_topics.ods erläutert.

const char *TOPICCommand                 = "d15/set/#";
const char *TOPICCommandDebug            = "d15/debugset/#";
const char *TOPICCmdFan1Speed            = "d15/set/kwl/fan1/standardspeed";
const char *TOPICCmdFan2Speed            = "d15/set/kwl/fan2/standardspeed";
const char *TOPICCmdGetSpeed             = "d15/set/kwl/fans/getspeed";
const char *TOPICCmdGetTemp              = "d15/set/kwl/temperatur/gettemp";
const char *TOPICCmdMode                 = "d15/set/kwl/lueftungsstufe";
const char *TOPICCmdAntiFreezeHyst       = "d15/set/kwl/antifreeze/hysterese";
const char *TOPICCmdBypassGetValues      = "d15/set/kwl/summerbypass/getvalues";
const char *TOPICCmdBypassManualFlap     = "d15/set/kwl/summerbypass/flap";
const char *TOPICCmdBypassMode           = "d15/set/kwl/summerbypass/mode";

const char *TOPICHeartbeat               = "d15/state/kwl/heartbeat";
const char *TOPICFan1Speed               = "d15/state/kwl/fan1/speed";
const char *TOPICFan2Speed               = "d15/state/kwl/fan2/speed";
const char *TOPICKwlOnline               = "d15/state/kwl/heartbeat";
const char *TOPICStateKwlMode            = "d15/state/kwl/lueftungsstufe";
const char *TOPICKwlTemperaturAussenluft = "d15/state/kwl/aussenluft/temperatur";
const char *TOPICKwlTemperaturZuluft     = "d15/state/kwl/zuluft/temperatur";
const char *TOPICKwlTemperaturAbluft     = "d15/state/kwl/abluft/temperatur";
const char *TOPICKwlTemperaturFortluft   = "d15/state/kwl/fortluft/temperatur";
const char *TOPICKwlAntifreeze           = "d15/state/kwl/antifreeze";
const char *TOPICKwlBypassState          = "d15/state/kwl/summerbypass/flap";
const char *TOPICKwlBypassMode           = "d15/state/kwl/summerbypass/mode";

const char *TOPICKwlBypassTempAbluftMin     = "d15/state/kwl/summerbypass/TempAbluftMin";
const char *TOPICKwlBypassTempAussenluftMin = "d15/state/kwl/summerbypass/TempAussenluftMin";
const char *TOPICKwlBypassHystereseMinutes  = "d15/state/kwl/summerbypass/HystereseMinutes";

// Die folgenden Topics sind nur für die SW-Entwicklung, und schalten Debugausgaben per mqtt ein und aus
const char *TOPICKwlDebugsetFan1Getvalues = "d15/debugset/kwl/fan1/getvalues";
const char *TOPICKwlDebugsetFan2Getvalues = "d15/debugset/kwl/fan2/getvalues";
const char *TOPICKwlDebugstateFan1        = "d15/debugstate/kwl/fan1";
const char *TOPICKwlDebugstateFan2        = "d15/debugstate/kwl/fan2";

// Die folgenden Topics sind nur für die SW-Entwicklung, es werden Messwerte überschrieben, es kann damit der Sommer-Bypass und die Frostschutzschaltung getestet werden
const char *TOPICKwlDebugsetTemperaturAussenluft = "d15/debugset/kwl/aussenluft/temperatur";
const char *TOPICKwlDebugsetTemperaturZuluft     = "d15/debugset/kwl/zuluft/temperatur";
const char *TOPICKwlDebugsetTemperaturAbluft     = "d15/debugset/kwl/abluft/temperatur";
const char *TOPICKwlDebugsetTemperaturFortluft   = "d15/debugset/kwl/fortluft/temperatur";
// Ende Topics

// Sind die folgenden Variablen auf true, wenn beim nächsten Durchlauf die entsprechenden mqtt Messages gesendet,
// anschliessend wird die Variable wieder auf false gesetzt
boolean mqttCmdSendTemp = false;
boolean mqttCmdSendFans = false;
boolean mqttCmdSendBypassState = false;
boolean mqttCmdSendBypassAllValues = false;
boolean mqttCmdSendMode = false;
// mqttDebug Messages
boolean mqttCmdSendAlwaysDebugFan1 = true;
boolean mqttCmdSendAlwaysDebugFan2 = true;
//

char   TEMPChar[10]; // Hilfsvariable zu Konvertierung
char   buffer[7];    // the ASCII of the integer will be stored in this char array
String TEMPAsString; // Ausgelesene Wert als String

// Variablen für Lüfter Tacho
double speedTachoFan1 = 0;    // Zuluft U/min
double speedTachoFan2 = 0;    // Abluft U/min
double SendMqttSpeedTachoFan1 = 0;
double SendMqttSpeedTachoFan2 = 0;
volatile long tachoFan1TimeSum = 0;
volatile long tachoFan2TimeSum = 0;
volatile byte tachoFan1CountSum = 0;
volatile byte tachoFan2CountSum = 0;
unsigned long tachoFan1LastMillis = 0;
unsigned long tachoFan2LastMillis = 0;

int cycleFan1Counter = 0;
int cycleFan2Counter = 0;

double StandardSpeedSetpointFan1 = 0;      // Solldrehzahl im U/min für Zuluft bei kwlMode = 3 (Standardlüftungsstufe), Drehzahlen werden aus EEPROM gelesen.
double StandardSpeedSetpointFan2 = 0;      // Solldrehzahl im U/min für Abluft bei kwlMode = 3 (Standardlüftungsstufe)
double speedSetpointFan1 = 0;              // Solldrehzahl im U/min für Zuluft bei Berücksichtungs der Lüftungsstufe
double speedSetpointFan2 = 0;              // Solldrehzahl im U/min für Zuluft bei Berücksichtungs der Lüftungsstufe
double techSetpointFan1  = 0;              // PWM oder Analogsignal 0..1000 für Zuluft
double techSetpointFan2  = 0;              // PWM oder Analogsignal 0..1000 für Abluft
// Ende Variablen für Lüfter


// Mode 0: Aus
// mode 1: 60%
// mode 2: 80%
// mode 3: 100%
// mode 4: 120%
// mode 5: 140%
int kwlMode = 3;      // Standardlüftungsstufe

boolean antifreezeState = false;
float   antifreezeTemp = 2.0;
int     antifreezeHyst = 3; 
double  antifreezeTempUpperLimit = antifreezeTemp + antifreezeHyst;
boolean antifreezeAlarm = false;

double        techSetpointPreheater = 0.0;     // Analogsignal 0..1000 für Vorheizer
unsigned long PreheaterStartMillis = 0;        // Beginn der Vorheizung

// Definitionen für das Scheduling
unsigned long intervalNtpTime = 1000;
unsigned long intervalTachoFan = 1000;
unsigned long intervalSetFan = 1000;
unsigned long intervalTempRead = 5000;                       // Abfrage Temperatur, muss größer als 1000 sein
unsigned long intervalAntifreezeCheck = 10 * 1000;  // 60 * 1000         // Frostschutzprüfung je Minute
unsigned long intervalAntifreezeAlarmCheck = 1 * 60 * 1000; // 10 * 60 * 1000; // Zeitraum zur Überprüfung, ob Vorheizregister die Temperatur erhöhen kann, 10 Min
unsigned long intervalBypassSummerCheck = 1 * 60 * 1000;     // Zeitraum zum Check der Bedingungen für BypassSummerCheck, 1 Minuten
unsigned long intervalBypassSummerSetFlaps = 5 * 60 * 1000;  // Zeitraum zum Setzen des Bypasses, 5 Minuten

unsigned long intervalMqttFan = 5000;
unsigned long intervalMqttMode = 5 * 60 * 1000;;
unsigned long intervalMqttTemp = 5000;
unsigned long intervalMqttTempOversampling = 5 * 60 * 1000;
unsigned long intervalMqttFanOversampling = 5 * 60 * 1000;
unsigned long intervalMqttBypassState = 15 * 60 * 1000;

unsigned long previousMillisFan = 0;
unsigned long previousMillisSetFan = 0;
unsigned long previousMillisAntifreeze = 0;
unsigned long previousMillisBypassSummerCheck = 0;
unsigned long previousMillisBypassSummerSetFlaps = 0;

unsigned long previousMillisMqttHeartbeat = 0;
unsigned long previousMillisMqttFan = 0;
unsigned long previousMillisMqttMode = 0;
unsigned long previousMillisMqttFanOversampling = 0;
unsigned long previousMillisMqttTemp = 0;
unsigned long previousMillisMqttTempOversampling = 0;
unsigned long previousMillisMqttBypassState = 0;

unsigned long previous100Millis = 0;
unsigned long previousMillisNtpTime = 0;
unsigned long previousMillisTemp = 0;
unsigned long currentMillis = 0;

// Start - Variablen für Bypass ///////////////////////////////////////////
#define bypassMode_Manual 1
#define bypassMode_Auto   0
#define bypassFlapState_Unknown 0
#define bypassFlapState_Close   1
#define bypassFlapState_Open    2

int  bypassManualSetpoint        = bypassFlapState_Close;             // Standardstellung Bypass geschlossen
int  bypassMode                  = bypassMode_Auto;                   // Automatische oder Manuelle Steuerung der Bypass-Klappe
int  bypassFlapState             = bypassFlapState_Unknown;           // aktuelle Stellung der Bypass-Klappe
int  bypassFlapStateDriveRunning = bypassFlapState_Unknown;
int  bypassTempAbluftMin         = 0;
int  bypassTempAussenluftMin     = 0;
int  bypassHystereseMinutes      = 60;
int  bypassFlapSetpoint          = bypassFlapState_Close;
unsigned long bypassLastChangeMillis   = 0;                       // Letzte Änderung für Hysterese

long          bypassFlapsDriveTime = 120 * 1000;                  // Fahrzeit (s) der Klappe zwischen den Stellungen Open und Close
boolean       bypassFlapsRunning = false;                         // True, wenn Klappe fährt
unsigned long bypassFlapsStartTime = 0;                           // Startzeit für den Beginn Klappenwechsels
// Ende  - Variablen für Bypass ///////////////////////////////////////////

// Begin EEPROM
#define    WRITE_EEPROM false // true = Werte im nichtflüchtigen Speicherbereich LÖSCHEN, false nichts tun
const int  BUFSIZE = 50;
char       eeprombuffer[BUFSIZE];

const int  EEPROM_MIN_ADDR = 0;
const int  EEPROM_MAX_ADDR = 1023;
// Ende EEPROM

// Ntp Zeit
EthernetUDP ntpUDP;
NTPClient timeClient(ntpUDP);

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

long lastReconnectAttempt = 0;

double TEMP1_Aussenluft = -127.0;    // Temperatur Außenluft
double TEMP2_Zuluft =     -127.0;    // Temperatur Zuluft
double TEMP3_Abluft =     -127.0;    // Temperatur Abluft
double TEMP4_Fortluft =   -127.0;    // Temperatur Fortluft

// PID REGLER
// Define the aggressive and conservative Tuning Parameters
// Nenndrehzahl Lüfter 3200, Stellwert 0..1000 entspricht 0-10V
double aggKp  = 0.5,  aggKi = 0.1, aggKd  = 0.001;
double consKp = 0.1, consKi = 0.1, consKd = 0.001;

double heaterKp = 5, heaterKi = 0.005, heaterKd = 0.025;

//Specify the links and initial tuning parameters
PID PidFan1(&speedTachoFan1, &techSetpointFan1, &speedSetpointFan1, consKp, consKi, consKd, P_ON_M, DIRECT );
PID PidFan2(&speedTachoFan2, &techSetpointFan2, &speedSetpointFan2, consKp, consKi, consKd, P_ON_M, DIRECT );

PID PidPreheater(&TEMP4_Fortluft, &techSetpointPreheater, &antifreezeTempUpperLimit, heaterKp, heaterKi, heaterKd, P_ON_M, DIRECT);
///////////////////////

// Temperatur Sensoren, Pinbelegung steht oben
OneWire Temp1OneWire(TEMP1_ONE_WIRE_BUS); // Einrichten des OneWire Bus um die Daten der Temperaturfühler abzurufen
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


// Ende Definition
///////////////////////////////////////


void mqttReceiveMsg(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String topicStr = topic;

  // Set Values
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
    kwlMode = i;
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
  if (topicStr == TOPICCmdBypassManualFlap) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "open")  {bypassManualSetpoint = bypassFlapState_Open; }
    if (s == "close") {bypassManualSetpoint = bypassFlapState_Close;}
    // Stellung Bypassklappe bei manuellem Modus
  }  
  if (topicStr == TOPICCmdBypassMode) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "auto")   {bypassMode = bypassMode_Auto;   mqttCmdSendBypassState=true;}
    if (s == "manual") {bypassMode = bypassMode_Manual; mqttCmdSendBypassState=true;}
    // Auto oder manueller Modus
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


  // Debug Messages, den folgenden Block in der produktiven Version auskommentieren
  if (topicStr == TOPICKwlDebugsetTemperaturAussenluft) {
    payload[length] = '\0';
    TEMP1_Aussenluft = String((char*)payload).toFloat();
    mqttCmdSendTemp = true;
  }
  if (topicStr == TOPICKwlDebugsetTemperaturZuluft) {
    payload[length] = '\0';
    TEMP2_Zuluft = String((char*)payload).toFloat();
    mqttCmdSendTemp = true;
  }
  if (topicStr == TOPICKwlDebugsetTemperaturAbluft) {
    payload[length] = '\0';
    TEMP3_Abluft = String((char*)payload).toFloat();
    mqttCmdSendTemp = true;
  }
  if (topicStr == TOPICKwlDebugsetTemperaturFortluft) {
    payload[length] = '\0';
    TEMP4_Fortluft = String((char*)payload).toFloat();
    mqttCmdSendTemp = true;
  }

    if (topicStr == TOPICKwlDebugsetFan1Getvalues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "on")   {mqttCmdSendAlwaysDebugFan1 = true;}
    if (s == "off")  {mqttCmdSendAlwaysDebugFan1 = false;}
  } 
  if (topicStr == TOPICKwlDebugsetFan2Getvalues) {
    payload[length] = '\0';
    String s = String((char*)payload);
    if (s == "on")   {mqttCmdSendAlwaysDebugFan2 = true;}
    if (s == "off")  {mqttCmdSendAlwaysDebugFan2 = false;}
  } 
  // Debug Messages, bis hier auskommentieren
   
}

boolean mqttReconnect() {
  Serial.println ("reconnect start");
  Serial.println ((long)currentMillis);
  if (mqttClient.connect("arduinoClientKwl", TOPICHeartbeat, 0, true, "offline")) {
    // Once connected, publish an announcement...
    mqttClient.publish(TOPICHeartbeat, "online");
    // ... and resubscribe
    mqttClient.subscribe(TOPICCommand);
    mqttClient.subscribe(TOPICCommandDebug);
  }
  Serial.println ("reconnect end");
  Serial.println ((long)currentMillis);
  return mqttClient.connected();
}

void setSpeedToFan() {
  // Ausgabewert für Lüftersteuerung darf zwischen 0-10V liegen, dies entspricht 0..1023, vereinfacht 0..1000
  // 0..1000 muss umgerechnet werden auf 0..255 also durch 4 geteilt werden
  // max. Lüfterdrehzahl bei Pabstlüfter 3200 U/min
  // max. Drehzahl 2300 U/min bei Testaufbau (alten Prozessorlüftern)

  double KwlModeFactor = 1.0;
  if (kwlMode == 0) {
    KwlModeFactor = 0;
  }
  if (kwlMode == 1) {
    KwlModeFactor = 0.6;
  }
  if (kwlMode == 2) {
    KwlModeFactor = 0.8;
  }
  if (kwlMode == 3) {
    KwlModeFactor = 1.0;
  }
  if (kwlMode == 4) {
    KwlModeFactor = 1.2;
  }
  if (kwlMode == 5) {
    KwlModeFactor = 1.4;
  }

  speedSetpointFan1 = StandardSpeedSetpointFan1 * KwlModeFactor;
  speedSetpointFan2 = StandardSpeedSetpointFan2 * KwlModeFactor;

  double gap1 = abs(speedSetpointFan1 - speedTachoFan1); //distance away from setpoint
  double gap2 = abs(speedSetpointFan2 - speedTachoFan2); //distance away from setpoint

  if (gap1 < 20) {
    PidFan1.SetTunings(consKp, consKi, consKd);
  }  else   {
    PidFan1.SetTunings(aggKp, aggKi, aggKd);
  }
  if (gap2 < 20) {
    PidFan2.SetTunings(consKp, consKi, consKd);
  }  else   {
    PidFan2.SetTunings(aggKp, aggKi, aggKd);
  }

  PidFan1.Compute();
  PidFan2.Compute();

  Serial.print ("mqttCmdSendAlwaysDebugFan1: ");
  Serial.println(mqttCmdSendAlwaysDebugFan1);
  
  if (mqttCmdSendAlwaysDebugFan1) { mqtt_debug_fan1(); }
  if (mqttCmdSendAlwaysDebugFan2) { mqtt_debug_fan2(); }
  
  if (serialDebugFan == 1){
    Serial.print ("Timestamp: ");
    Serial.println ((long)millis());
    Serial.print ("Fan 1: ");
    Serial.print ("\tGap: ");
    Serial.print (gap1);
    Serial.print ("\tspeedTachoFan1: ");
    Serial.print (speedTachoFan1);
    Serial.print ("\ttechSetpointFan1: ");
    Serial.print (techSetpointFan1);
    Serial.print ("\tspeedSetpointFan1: ");
    Serial.println(speedSetpointFan1);
  
    Serial.print ("Fan 2: ");
    Serial.print ("\tGap: ");
    Serial.print (gap2);
    Serial.print ("\tspeedTachoFan2: ");
    Serial.print (speedTachoFan2);
    Serial.print ("\ttechSetpointFan2: ");
    Serial.print (techSetpointFan2);
    Serial.print ("\tspeedSetpointFan2: ");
    Serial.println(speedSetpointFan2);
  }

  if (kwlMode == 0)               {
    techSetpointFan1 = 0 ;  // Lüfungsstufe 0 alles ausschalten
    techSetpointFan2 = 0;
  }
  if ((TEMP4_Fortluft <= -1)
      && (TEMP4_Fortluft != -127.0))       {
    techSetpointFan2 = 0; // Frostschutzalarm bei -1.0 wird der Zuluftventilator abgeschaltet
  }

  SetPreheating();

  if (serialDebugFan == 1){
    Serial.print ("techSetpointFan2: ");
    Serial.println (techSetpointFan2);
  }

  analogWrite(pwmPinFan1, techSetpointFan1 / 4);
  analogWrite(pwmPinFan2, techSetpointFan2 / 4);
}

void SetPreheating() {
  // Das Vorheizregister wird durch ein PID geregelt
  // Die Stellgröße das zwischen 0..10V liegen
  // Wenn der Zuluftventilator unter einer Schwelle des Tachosignals liegt, wird das Vorheizregister IMMER ausgeschaltet (SICHERHEIT)
  // Schwelle: 1200 U/min

  if (serialDebugAntifreeze == 1) {Serial.println ("SetPreheating start");}
  if (antifreezeState) {
    if (serialDebugAntifreeze == 1) {Serial.println ("antifreezeState true");}
    if (!antifreezeAlarm) {
      if (serialDebugAntifreeze == 1) {Serial.println ("Vorheizer versuchen");}
      // Vorheizer versuchen
      if (PidPreheater.GetMode() == MANUAL) {
        PidPreheater.SetMode(AUTOMATIC);  // Pid einschalten
      }
      PidPreheater.Compute();
    } else {
      // antifreezeAlarm == true also 10 Minuten vergangen seit antifreezeState == true
      // jetzt pidPreheater ausschalten,techSetpointPreheater=0 und Zuluftventilator ausschalten
      if (serialDebugAntifreeze == 1) {Serial.println ("antifreezeAlarm == true also 10 Minuten vergangen seit antifreezeState == true");}
      if (PidPreheater.GetMode() == AUTOMATIC) {
        PidPreheater.SetMode(MANUAL); // Pid ausschalten
      }
      if (serialDebugAntifreeze == 1) {Serial.println ("fan2 = 0");}
      techSetpointFan2 = 0;
      techSetpointPreheater = 0;
    }
  } else {
    if (PidPreheater.GetMode() == AUTOMATIC) {
      PidPreheater.SetMode(MANUAL); // Pid ausschalten
    }
    techSetpointPreheater = 0;
  }

  // Sicherheitsabfrage
  if (speedTachoFan2 < 1200 || (techSetpointFan2 == 0) )
  { // Sicherheitsabschaltung Vorheizer unter 1000 Umdrehungen Zuluftventilator
    techSetpointPreheater = 0;
  }

  analogWrite(pwmPinPreheater, techSetpointPreheater / 4);
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
    t = Temp1Sensor.getTempCByIndex(0); if (t > -127.0) {TEMP1_Aussenluft = t;}
    t = Temp2Sensor.getTempCByIndex(0); if (t > -127.0) {TEMP2_Zuluft = t;}
    t = Temp3Sensor.getTempCByIndex(0); if (t > -127.0) {TEMP3_Abluft = t;}
    t = Temp4Sensor.getTempCByIndex(0); if (t > -127.0) {TEMP4_Fortluft = t;}
  }
}

void loopAntiFreezeCheck() {
  currentMillis = millis();
  if (currentMillis - previousMillisAntifreeze >= intervalAntifreezeCheck) {
    if (serialDebugAntifreeze == 1) {Serial.println ("loopAntiFreezeCheck start");}
     previousMillisAntifreeze = currentMillis;
    // Antifreeze Flag einschalten
    if ((TEMP4_Fortluft <= antifreezeTemp) && (TEMP1_Aussenluft < 0.0)
        && (TEMP4_Fortluft > -127.0) && (TEMP1_Aussenluft > -127.0))         // Wenn Sensoren fehlen, ist der Wert -127
    {
      if (serialDebugAntifreeze == 1) {Serial.println ("Antifreeze Flag einschalten?");}
      if (!antifreezeState) {
        // Nur bei Zustandsänderung Senden anstossen und PID einschalten
        if (serialDebugAntifreeze == 1) {Serial.println ("Antifreeze Flag einschalten");}
        mqttCmdSendTemp = true;
        antifreezeState = true;
        PreheaterStartMillis = millis();
      }
    }
    // Antifreeze Flag ausschalten
    if (TEMP4_Fortluft > antifreezeTemp + antifreezeHyst) {
      if (serialDebugAntifreeze == 1) {Serial.println ("Antifreeze Flag ausschalten?");}
      if (antifreezeState) {
        // Nur bei Zustandsänderung Senden anstossen
        if (serialDebugAntifreeze == 1) {Serial.println ("Antifreeze Flag ausschalten");}
        mqttCmdSendTemp = true;
      }
      // AntifreezeState und Alarm aufheben, Tempertatur liegt im "normalen" Bereich
      antifreezeState = false;
      antifreezeAlarm = false;
      PreheaterStartMillis = 0;
    }
    if (antifreezeState && (millis() - PreheaterStartMillis >= intervalAntifreezeAlarmCheck)) {
      if (serialDebugAntifreeze == 1) {Serial.println ("Antifreeze antifreezeAlarm Checktemp");}
      if (TEMP4_Fortluft <= antifreezeTemp) {
        if (serialDebugAntifreeze == 1) {Serial.println ("Antifreeze antifreezeAlarm");}
        // Temperatur konnte auch nach 10 Minuten nicht erhöht werden, Alarm setzen
        antifreezeAlarm = true;
      }
    }
  }
}

void loopBypassSummerCheck() {
  // Bedingungen für Sommer Bypass überprüfen und Variable ggfs setzen
  currentMillis = millis();
  if (currentMillis - previousMillisBypassSummerCheck >= intervalBypassSummerCheck) {    
    previousMillisBypassSummerCheck = currentMillis;    
    // Auto oder Manual?
    if (bypassMode == bypassMode_Auto) {
      // Automatic
      // Hysterese überprüfen
      if (currentMillis - bypassLastChangeMillis >= (bypassHystereseMinutes * 60 * 1000)) {
        if ((TEMP1_Aussenluft    < TEMP3_Abluft - 2)
            && (TEMP3_Abluft     > bypassTempAbluftMin)
            && (TEMP1_Aussenluft > bypassTempAussenluftMin)) {
          //ok, dann Klappe öffen
          if (bypassFlapSetpoint != bypassFlapState_Open) {
            bypassFlapSetpoint = bypassFlapState_Open;
            bypassLastChangeMillis = millis();
          }
        } else {
          //ok, dann Klappe schliessen
          if (bypassFlapSetpoint != bypassFlapState_Close) {
            bypassFlapSetpoint = bypassFlapState_Close;
            bypassLastChangeMillis = millis();
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
    previousMillisBypassSummerSetFlaps = currentMillis;
    if (bypassFlapSetpoint != bypassFlapState) {    // bypassFlapState wird NACH erfolgter Fahrt gesetzt
      if ((bypassFlapsStartTime == 0)  || (millis() - bypassFlapsStartTime > bypassFlapsDriveTime)) {
        if (!bypassFlapsRunning) {
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
          // Klappe wurde gefahren, jetzt abschalten
          // Realis ausschalten
          // Erst Power, dann Richtung beim Ausschalten
          digitalWrite(relPinBypassPower, RELAY_OFF);
          digitalWrite(relPinBypassDirection, RELAY_OFF);

          bypassFlapsRunning = false;
          bypassFlapState = bypassFlapStateDriveRunning;
          bypassLastChangeMillis = millis();
          // MQTT senden
          mqttCmdSendBypassState = true;
        }
      }
    }
  }
}

void loopNtpTime() {
  currentMillis = millis();
  if (currentMillis - previousMillisNtpTime >= intervalNtpTime) {
    previousMillisNtpTime = currentMillis;
    timeClient.update();
    Serial.println(timeClient.getFormattedTime());
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

    if (_tachoFan1TimeSum != 0) {
      speedTachoFan1 = (float)_cycleFan1Counter * 60 / ((float)(_tachoFan1TimeSum) / 1000.0);  // Umdrehungen pro Minute
    } else {
      speedTachoFan1 = 0;
    }
    if (_tachoFan2TimeSum != 0) {
      speedTachoFan2 = (float)_cycleFan2Counter * 60 / ((float)(_tachoFan2TimeSum) / 1000.0);
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
    setSpeedToFan();
  }
}


// loopMqtt... senden Werte an den mqtt-Server.

void loopMqttSendFan() {
  // Senden der Drehzahlen per Mqtt
  // Bedingung: a) alle x Sekunden, wenn Differenz Geschwindigkeit > 100
  //            b) alle intervalMqttFanOversampling/1000 Sekunden (Standard 5 Minuten)
  //            c) mqttCmdSendFans == true
  currentMillis = millis();
  if ((currentMillis - previousMillisMqttFan > intervalMqttFan) || mqttCmdSendFans) {
    previousMillisMqttFan = currentMillis;
    if ((abs(speedTachoFan1 - SendMqttSpeedTachoFan1) > 100)
        || (abs(speedTachoFan2 - SendMqttSpeedTachoFan2) > 100)
        || (currentMillis - previousMillisMqttFanOversampling > intervalMqttFanOversampling)
        || mqttCmdSendFans)  {

      mqttCmdSendFans = false;
      SendMqttSpeedTachoFan1 = speedTachoFan1;
      SendMqttSpeedTachoFan2 = speedTachoFan2;
      previousMillisMqttFanOversampling = currentMillis;

      itoa(speedTachoFan1, buffer, 10); //(integer, yourBuffer, base)
      mqttClient.publish(TOPICFan1Speed, buffer);
      if (serialDebug == 1) {
        Serial.println("speedTachoFan1: " + String(speedTachoFan1));
      }
      itoa(speedTachoFan2, buffer, 10); //(integer, yourBuffer, base)
      mqttClient.publish(TOPICFan2Speed, buffer);
      if (serialDebug == 1) {
        Serial.println("speedTachoFan2: " + String(speedTachoFan2));
      }
    }
  }
}

void loopMqttSendMode() {
  // Senden der Lüftungsstufe
  // Bedingung: a) alle x Sekunden (Standard 5 Minuten)
  //            b) mqttCmdSendMode == true
  currentMillis = millis();
  if ((currentMillis - previousMillisMqttMode > intervalMqttMode) || mqttCmdSendMode) {
    previousMillisMqttMode = currentMillis;
    mqttCmdSendMode = false;
    itoa(kwlMode, buffer, 10);
    mqttClient.publish(TOPICStateKwlMode, buffer);
  }
}

void loopMqttSendBypass() {
  // Senden der Bypass - Einstellung per Mqtt
  // Bedingung: a) alle x Sekunden
  //            b) mqttCmdSendBypassState == true
  currentMillis = millis();
  if ((currentMillis - previousMillisMqttBypassState > intervalMqttBypassState) || mqttCmdSendBypassState || mqttCmdSendBypassAllValues) {
    previousMillisMqttBypassState = currentMillis;
    mqttCmdSendBypassState = false;

    if (bypassFlapState == bypassFlapState_Open) {
      mqttClient.publish(TOPICKwlBypassState, "open");
      if (serialDebug == 1) {
        Serial.println("TOPICKwlBypassState: open");
      }
    } else if (bypassFlapState == bypassFlapState_Close) {
      mqttClient.publish(TOPICKwlBypassState, "closed");
      if (serialDebug == 1) {
        Serial.println("TOPICKwlBypassState: closed");
      }
    } else if (bypassFlapState == bypassFlapState_Unknown) {
      mqttClient.publish(TOPICKwlBypassState, "unknown");
      if (serialDebug == 1) {
        Serial.println("TOPICKwlBypassState: unknown");
      }
    }
    // In MqttSendBypassAllValues() wird geprüft, ob alle Werte gesendet werden sollen.
    MqttSendBypassAllValues();
  }
}


void MqttSendBypassAllValues() {
  // Senden der Bypass - Einstellung per Mqtt
  // Bedingung: a) mqttCmdSendBypassAllValues == true
  if (mqttCmdSendBypassAllValues) {
    mqttCmdSendBypassAllValues = false;

    if (bypassMode == bypassMode_Auto) {
      mqttClient.publish(TOPICKwlBypassMode, "auto");
      if (serialDebug == 1) {
        Serial.println("TOPICKwlBypassMode: auto");
      }
    } else if (bypassFlapState == bypassMode_Manual) {
      mqttClient.publish(TOPICKwlBypassMode, "manual");
      if (serialDebug == 1) {
        Serial.println("TOPICKwlBypassMode: manual");
      }
    }
    itoa(bypassTempAbluftMin, buffer, 10);
    mqttClient.publish(TOPICKwlBypassTempAbluftMin, buffer);
    if (serialDebug == 1) {
      Serial.println("TOPICKwlBypassTempAbluftMin: " + String(bypassTempAbluftMin));
    }
    itoa(bypassTempAussenluftMin, buffer, 10);
    mqttClient.publish(TOPICKwlBypassTempAussenluftMin, buffer);
    if (serialDebug == 1) {
      Serial.println("TOPICKwlBypassTempAussenluftMin: " + String(bypassTempAussenluftMin));
    }
    itoa(bypassHystereseMinutes, buffer, 10);
    mqttClient.publish(TOPICKwlBypassHystereseMinutes, buffer);
    if (serialDebug == 1) {
      Serial.println("TOPICKwlBypassHystereseMinutes: " + String(bypassHystereseMinutes));
    }
  }
}


void loopMqttSendTemp() {
  // Senden der Temperaturen per Mqtt
  // Bedingung: a) alle x Sekunden, wenn Differenz Temperatur > 0.5
  //            b) alle intervalMqttTempOversampling/1000 Sekunden (Standard 5 Minuten)
  //            c) mqttCmdSendTemp == true
  currentMillis = millis();
  if ((currentMillis - previousMillisMqttTemp > intervalMqttTemp) || mqttCmdSendTemp) {
    previousMillisMqttTemp = currentMillis;
    if ((abs(TEMP1_Aussenluft - SendMqttTEMP1) > 0.5)
        || (abs(TEMP2_Zuluft - SendMqttTEMP2) > 0.5)
        || (abs(TEMP3_Abluft - SendMqttTEMP3) > 0.5)
        || (abs(TEMP4_Fortluft - SendMqttTEMP4) > 0.5)
        || (currentMillis - previousMillisMqttTempOversampling > intervalMqttTempOversampling)
        || mqttCmdSendTemp)  {

      mqttCmdSendTemp = false;
      SendMqttTEMP1 = TEMP1_Aussenluft;
      SendMqttTEMP2 = TEMP2_Zuluft;
      SendMqttTEMP3 = TEMP3_Abluft;
      SendMqttTEMP4 = TEMP4_Fortluft;
      previousMillisMqttTempOversampling = currentMillis;

      dtostrf(TEMP1_Aussenluft, 6, 2, buffer);
      mqttClient.publish(TOPICKwlTemperaturAussenluft, buffer);
      if (serialDebug == 1) {
        Serial.println("TOPICKwlTemperaturAussenluft: " + String(TEMP1_Aussenluft));
      }
      dtostrf(TEMP2_Zuluft, 6, 2, buffer);
      mqttClient.publish(TOPICKwlTemperaturZuluft, buffer);
      if (serialDebug == 1) {
        Serial.println("TOPICKwlTemperaturZuluft: " + String(TEMP2_Zuluft));
      }
      dtostrf(TEMP3_Abluft, 6, 2, buffer);
      mqttClient.publish(TOPICKwlTemperaturAbluft, buffer);
      if (serialDebug == 1) {
        Serial.println("TOPICKwlTemperaturAbluft: " + String(TEMP1_Aussenluft));
      }
      dtostrf(TEMP4_Fortluft, 6, 2, buffer);
      mqttClient.publish(TOPICKwlTemperaturFortluft, buffer);
      if (serialDebug == 1) {
        Serial.println("TOPICKwlTemperaturFortluft: " + String(TEMP2_Zuluft));
      }
      if (antifreezeState) {
        mqttClient.publish(TOPICKwlAntifreeze, "on");
      } else {
        mqttClient.publish(TOPICKwlAntifreeze, "off");
      }
    }
  }
}

void loopMqttHeartbeat(){
  if (millis() - previousMillisMqttHeartbeat > 30000){
    previousMillisMqttHeartbeat = millis();
    mqttClient.publish(TOPICHeartbeat, "online");
  }
}

void loopMqttConnection() {
  // mqtt Client connected?
  if (!mqttClient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      //if (serialDebug == 1) {
      Serial.println("Client not connected");
      //}
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (mqttReconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    mqttClient.loop();
  }
}

// LOOP SCHLEIFE Ende

/**********************************************************************
  Setup Routinen
**********************************************************************/

/****************************************
  Werte auslesen und Variablen zuweisen *
  **************************************/
void initializeVariables()
{
  Serial.println();
  Serial.println("initializeVariables");
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
  start_tft();
  print_header();
  print_footer();
   
  initializeEEPROM();
  initializeVariables();

  Serial.println();
  Serial.println("Booting...");
  tft.setCursor(0, 30);
  tft.println("Booting...");
  mqttClient.setServer(mqttserver, 1883);
  mqttClient.setCallback(mqttReceiveMsg);

  Ethernet.begin(mac, ip);
  delay(1500);    // Delay in Setup erlaubt
  lastReconnectAttempt = 0;
  
  Serial.println("Warte auf Zeitsynchronisation");
  tft.println("Warte auf Zeitsynchronisation");
  
  // NTP Client starten
  timeClient.begin(); // NTP Provider starten / erstmalig abfragen

  Serial.print("Server Adresse: ");
  Serial.println(Ethernet.localIP());
  tft.print("Server Adresse: ");
  tft.println(Ethernet.localIP());

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

  // Lüfter Speed
  pinMode(pwmPinFan1, OUTPUT);
  digitalWrite(pwmPinFan1, LOW);
  pinMode(pwmPinFan2, OUTPUT);
  digitalWrite(pwmPinFan2, LOW);

  // Lüfter Tacho Interrupt
  Serial.print("Teste Ventilatoren");
  tft.println("Teste Ventilatoren");
  pinMode(tachoPinFan1, INPUT_PULLUP);
  attachInterrupt(tachoPinFan1, countUpFan1, RISING );
  pinMode(tachoPinFan2, INPUT_PULLUP);
  attachInterrupt(tachoPinFan2, countUpFan2, RISING );

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

  // 4 Sekunden Pause für die TFT Anzeige, damit man sie auch lesen kann
  delay (4000); 

  //PID
  //turn the PID on
  PidFan1.SetOutputLimits(0, 1000);
  PidFan1.SetMode(AUTOMATIC);
  PidFan1.SetSampleTime(intervalSetFan);

  PidFan2.SetOutputLimits(0, 1000);
  PidFan2.SetMode(AUTOMATIC);
  PidFan2.SetSampleTime(intervalSetFan);

  PidPreheater.SetOutputLimits(0, 1000);
  PidPreheater.SetMode(MANUAL);
  PidPreheater.SetSampleTime(intervalSetFan);  // SetFan ruft Preheater auf, deswegen hier intervalSetFan

  previousMillisTemp = millis();

  Serial.println("Setup completed...");
  tft.println("Setup completed...");
  tft.fillRect(0, 30, 480, 200, BLACK);
}
// *** SETUP ENDE

// *** LOOP START ***
void loop()
{

  //loopWrite100Millis();
  loopMqttSendMode();
  loopMqttSendFan();
  loopMqttSendTemp();
  loopMqttSendBypass();

  //loopNtpTime();
  loopTachoFan();
  loopSetFan();
  loopAntiFreezeCheck();
  loopBypassSummerCheck();
  loopTemperaturRequest();
  loopTemperaturRead();

  loopMqttHeartbeat();
  loopMqttConnection();

}
// *** LOOP ENDE ***


