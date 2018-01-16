// Font einbinden
#include <Fonts/FreeSans9pt7b.h>  // Font
#include <Fonts/FreeSans12pt7b.h>  // Font
#include <Fonts/FreeSansBold24pt7b.h>
/* FARBEN


*/
/*
  #define colButton-BackColor-Pressed 0x41B3D6 //  65 179 214
  #define colButton-BackColor         0x3A9FBE //  58 159 190
  //#define colBackColor                0x000BFF //  45 127 151
  #define colBackColor                0xFFFFFF //  45 127 151
  //#define colBackColor                0x2D7F97 //  45 127 151
  #define colWindow-Title             0x42B6DA //  66 182 218
  #define colFontColor                0x000000 // 255 255 255
  //#define colFontColor                0xFFFFFF // 255 255 255
*/

// RGB in 565 Kodierung
// Colorpicker http://www.barth-dev.de/online/rgb565-color-picker/
// Weiss auf Schwarz
#define colBackColor                0x0000 //  45 127 151
#define colWindowTitleBackColor     0xFFFF //  66 182 218
#define colWindowTitleFontColor     0x0000 //  66 182 218
#define colFontColor                0xFFFF // 255 255 255
#define colErrorBackColor           0xF800 // rot
#define colInfoBackColor            0xFFE0 // gelb
#define colErrorFontColor           0xFFFF // weiss
#define colInfoFontColor            0x0000 // schwarz
#define colMenuBtnFrame             0x0000
#define colMenuBackColor            0xFFFF
#define colMenuFontColor            0x0000
#define colMenuBtnFrameHL           0xF800

/*
  // Schwarz auf weiss
  #define colBackColor                0xFFFFFF //  45 127 151
  #define colWindowTitleBackColor     0x000000 //  66 182 218
  #define colWindowTitleFontColor     0xFFFFFF //  66 182 218
  #define colFontColor                0x000000 // 255 255 255
*/

uint16_t identifier;
uint8_t Orientation = 3;    //PORTRAIT

#define       fontFactorSmall               1
#define       fontFactorBigNumber           3
int           baselineSmall               = 0;
int           baselineMiddle              = 0;
int           baselineBigNumber           = 0;
int           numberfieldheight           = 0;

unsigned long previousMillisDisplayUpdate = 0;
unsigned long intervalDisplayUpdate       = 5000;
boolean       updateDisplayNow            = false;
int           LastDisplaySpeedTachoFan1   = 0;
int           LastDisplaySpeedTachoFan2   = 0;
int           LastDisplaykwlMode          = 0;
int           LastEffiencyKwl             = 0;
String        LastErrorText               = "";
String        LastInfoText                = "";
double LastDisplayT1 = 0, LastDisplayT2 = 0, LastDisplayT3 = 0, LastDisplayT4 = 0;
byte          LastLanOk                   = true;
byte          LastMqttOk                  = true;

byte          touchBtnWidth               = 60;
byte          touchBtnHeight              = 45;
byte          touchBtnYOffset             = 30;


int16_t  x1, y1;
uint16_t w, h;

boolean       menuBtnActive[6];
byte          menupage                    = 0;
byte          menuBtnPressed              = -1;
byte          LastMenuBtn                 = 0;
unsigned long millisLastMenuBtnPress      = 0;
unsigned long intervalMenuBtn             = 500;

int inputStandardSpeedSetpointFan1 = 0;
int inputStandardSpeedSetpointFan2 = 0;


// ****************************************** Page 0: HAUPTSEITE ******************************************
void SetupBackgroundPage0() {
  // Menu Page 0 Standardseite

  tft.setFont(&FreeSans9pt7b);  // Kleiner Font

  tft.fillRect(0, 30, 480 - touchBtnWidth, 20, colWindowTitleBackColor );
  tft.setCursor(160, 30 + baselineSmall);
  tft.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor );
  tft.setTextSize(fontFactorSmall);
  tft.print(F("Lueftungsstufe"));
  tft.setTextColor(colFontColor, colBackColor );

  tft.setCursor(150, 140 + baselineSmall);
  tft.print(F("Temperatur"));

  tft.setCursor(270, 140 + baselineSmall);
  tft.print(F("Luefterdrehzahl"));

  tft.setCursor(18, 166 + baselineMiddle);
  tft.print(F("Aussenluft"));

  tft.setCursor(18, 192 + baselineMiddle);
  tft.print(F("Zuluft"));

  tft.setCursor(18, 218 + baselineMiddle);
  tft.print(F("Abluft"));

  tft.setCursor(18, 244 + baselineMiddle);
  tft.print(F("Fortluft"));

  tft.setCursor(18, 270 + baselineMiddle);
  tft.print(F("Wirkungsgrad"));
  // Ende Menu Page 1
}

void loopDisplayUpdatePage0() {
  char strPrint[10];

  // Menu Page 0
  if (LastDisplaykwlMode != kwlMode || updateDisplayNow) {
    // KWL Mode
    tft.setFont(&FreeSansBold24pt7b);
    tft.setTextColor(colFontColor, colBackColor);
    tft.setTextSize(2);

    tft.setCursor(200, 55 + 2 * baselineBigNumber);
    sprintf(strPrint, "%-1i", (int)kwlMode);
    tft.fillRect(200, 55, 60, 80, colBackColor);
    tft.print(strPrint);
    LastDisplaykwlMode = kwlMode;
    tft.setTextSize(1);
  }
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colFontColor, colBackColor);

  // Speed Fan1
  if (abs(LastDisplaySpeedTachoFan1 - speedTachoFan1) > 10 || updateDisplayNow) {
    LastDisplaySpeedTachoFan1 = speedTachoFan1;
    tft.fillRect(280, 192, 80, numberfieldheight, colBackColor);
    sprintf(strPrint, "%5i", (int)speedTachoFan1);
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(340 - w, 192 + baselineMiddle);
    tft.print(strPrint);
  }
  // Speed Fan2
  if (abs(LastDisplaySpeedTachoFan2 - speedTachoFan2) > 10 || updateDisplayNow) {
    LastDisplaySpeedTachoFan2 = speedTachoFan2;
    sprintf(strPrint, "%5i", (int)speedTachoFan2);
    // Debug einkommentieren
    // tft.fillRect(280, 218, 60, numberfieldheight, colWindowTitle);
    tft.fillRect(280, 218, 80, numberfieldheight, colBackColor);
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(340 - w, 218 + baselineMiddle);
    tft.print(strPrint);
  }
  // T1
  if (abs(LastDisplayT1 - TEMP1_Aussenluft) > 0.5 || updateDisplayNow) {
    LastDisplayT1 = TEMP1_Aussenluft;
    tft.fillRect(160, 166, 80, numberfieldheight, colBackColor);
    sprintf(strPrint, "%5d.%1d", (int)TEMP1_Aussenluft, abs(((int)(TEMP1_Aussenluft * 10)) % 10));
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(240 - w, 166 + baselineMiddle);
    tft.print(strPrint);
  }
  // T2
  if (abs(LastDisplayT2 - TEMP2_Zuluft) > 0.5 || updateDisplayNow) {
    LastDisplayT2 = TEMP2_Zuluft;
    tft.fillRect(160, 192, 80, numberfieldheight, colBackColor);
    sprintf(strPrint, "%5d.%1d", (int)TEMP2_Zuluft, abs(((int)(TEMP2_Zuluft * 10)) % 10));
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(240 - w, 192 + baselineMiddle);
    tft.print(strPrint);
  }
  // T3
  if (abs(LastDisplayT3 - TEMP3_Abluft) > 0.5 || updateDisplayNow) {
    LastDisplayT3 = TEMP3_Abluft;
    tft.fillRect(160, 218, 80, numberfieldheight, colBackColor);
    sprintf(strPrint, "%5d.%1d", (int)TEMP3_Abluft, abs(((int)(TEMP3_Abluft * 10)) % 10));
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(240 - w, 218 + baselineMiddle);
    tft.print(strPrint);
  }
  // T4
  if (abs(LastDisplayT4 - TEMP4_Fortluft) > 0.5 || updateDisplayNow) {
    LastDisplayT4 = TEMP4_Fortluft;
    tft.fillRect(160, 244, 80, numberfieldheight, colBackColor);
    sprintf(strPrint, "%5d.%1d", (int)TEMP4_Fortluft, abs(((int)(TEMP4_Fortluft * 10)) % 10));
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(240 - w, 244 + baselineMiddle);
    tft.print(strPrint);
  }
  // Etha Wirkungsgrad
  if (abs(LastEffiencyKwl - EffiencyKwl) > 1 || updateDisplayNow) {
    LastEffiencyKwl = EffiencyKwl;
    tft.fillRect(160, 270, 80, numberfieldheight, colBackColor);
    sprintf(strPrint, "%5d %%", (int)EffiencyKwl);
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(240 - w, 270 + baselineMiddle);
    tft.print(strPrint);
  }
}
void NewMenuPage0() {
  NewMenuEntry (1, F("->"));  // Führt zur Page 1
  NewMenuEntry (3, F("+"));
  NewMenuEntry (4, F("-"));
}

void DoMenuActionPage0() {
  // Standardseite
  switch (menuBtnPressed) {
    case 1:
      gotoPage (1);
      break;
    case 3:
      previousMillisDisplayUpdate = 0;
      if (kwlMode < defStandardModeCnt - 1)  kwlMode = kwlMode + 1;
      break;
    case 4:
      previousMillisDisplayUpdate = 0;
      if (kwlMode > 0)  kwlMode = kwlMode - 1;
      break;
    default:
      previousMillisDisplayUpdate = 0;
      break;
  }
}
// ************************************ ENDE: Page 0  *****************************************************

// ****************************************** Page 1: WEITERE SENSORWERTE *********************************
void SetupBackgroundPage1() {
  // Menu Page 1, Sensorwerte

  tft.setFont(&FreeSans9pt7b);  // Kleiner Font

  tft.fillRect(0, 30, 480 - touchBtnWidth, 20, colWindowTitleBackColor );
  tft.setCursor(100, 30 + baselineSmall);
  tft.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor );
  tft.setTextSize(fontFactorSmall);
  tft.print(F("Weitere Sensorwerte"));

  tft.setTextColor(colFontColor, colBackColor );

  tft.setCursor(18, 166 + baselineMiddle);
  tft.print("DHT 1");

  tft.setCursor(18, 192 + baselineMiddle);
  tft.print("DHT 2");

  tft.setCursor(18, 218 + baselineMiddle);
  tft.print("CO2");

  tft.setCursor(18, 244 + baselineMiddle);
  tft.print("VOC");
}

void NewMenuPage1() {
  //DHT, CO2, VOC Messwerte
  NewMenuEntry (1, F("->"));  // Führt zur Seite 2
  NewMenuEntry (2, F("<-"));  // Führt zur Seite 0
}

void DoMenuActionPage1() {
  // Weitere Sonsorwerte
  switch (menuBtnPressed) {
    case 1:
      gotoPage (2);
      break;
    case 2:
      gotoPage (0);
      break;
    default:
      previousMillisDisplayUpdate = 0;
      break;
  }
}

// ************************************ ENDE: Page 1  *****************************************************

// ****************************************** Page 2: EINSTELLUNGEN ÜBERSICHT******************************
void SetupBackgroundPage2() {
  // Übersicht Einstellungen
  tft.setFont(&FreeSans9pt7b);  // Kleiner Font

  tft.fillRect(0, 30, 480 - touchBtnWidth, 20, colWindowTitleBackColor );
  tft.setCursor(160, 30 + baselineSmall);
  tft.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor );
  tft.setTextSize(fontFactorSmall);
  tft.print(F("Einstellungen"));

  tft.setTextColor(colFontColor, colBackColor );

  tft.setCursor(18, 166 + baselineMiddle);
  tft.print (F("L1:  Normdrehzahl Zuluftventilator einstellen"));
  tft.setCursor(18, 192 + baselineMiddle);
  tft.print (F("L2:  Normdrehzahl Abluftventilator einstellen"));
  tft.setCursor(18, 218 + baselineMiddle);
  tft.print(F("CAL: Kalibrierung der Ventilatoransteuerung"));
  tft.setCursor(18, 244 + baselineMiddle);
  tft.print(F("RES: Zurücksetzen aller Einstellungen"));
}

void NewMenuPage2() {
  //Einstellungen Übersicht
  NewMenuEntry (1, F("->"));  // Führt zur Seite 0
  NewMenuEntry (2, F("L1"));  // Führt zur Seite 3
  NewMenuEntry (3, F("L2"));  // Führt zur Seite 4
  NewMenuEntry (4, F("CAL")); // Führt zur Seite 5
  NewMenuEntry (5, F("RES")); // Fährt zur Seite 6
}

void DoMenuActionPage2() {
  // Einstellungen Übersicht
  switch (menuBtnPressed) {
    case 1:
      gotoPage (0);
      break;
    case 2:
      gotoPage (3);
      break;
    case 3:
      gotoPage (4);
      break;
    case 4:
      gotoPage (5);
      break;
    case 5:
      gotoPage (6);
      break;
    default:
      previousMillisDisplayUpdate = 0;
      break;
  }
}
// ************************************ ENDE: Page 2  *****************************************************

// ****************************************** Page 3: EINSTELLUNG NORMDREHZAHL L1 *************************
void SetupPage3() {
  inputStandardSpeedSetpointFan1 = StandardSpeedSetpointFan1;
}

void SetupBackgroundPage3() {
  // Übersicht Einstellungen
  tft.setFont(&FreeSans9pt7b);  // Kleiner Font

  tft.fillRect(0, 30, 480 - touchBtnWidth, 20, colWindowTitleBackColor );
  tft.setCursor(160, 30 + baselineSmall);
  tft.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor );
  tft.setTextSize(fontFactorSmall);
  tft.print(F("Normdrehzahl L1"));

  tft.setTextColor(colFontColor, colBackColor );

  tft.setCursor(18, 166 + baselineMiddle);
  tft.print (F("Normdrehzahl Zuluftventilator"));
}

void loopDisplayUpdatePage3() {
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colFontColor, colBackColor);
  tft.fillRect(18, 192, 80, numberfieldheight, colBackColor);
  tft.setCursor(18, 192 + baselineMiddle);
  tft.print (inputStandardSpeedSetpointFan1);
}

void NewMenuPage3() {
  //Einstellungen L1
  NewMenuEntry (3, F("+10"));
  NewMenuEntry (4, F("-10"));
  NewMenuEntry (5, F("ESC"));
  NewMenuEntry (6, F("OK"));
}

void DoMenuActionPage3() {
  // L1
  switch (menuBtnPressed) {
    case 3:
      previousMillisDisplayUpdate = 0;
      inputStandardSpeedSetpointFan1 = inputStandardSpeedSetpointFan1 + 10;
      if (inputStandardSpeedSetpointFan1 > 10000) inputStandardSpeedSetpointFan1 = 10000;
      break;
    case 4:
      previousMillisDisplayUpdate = 0;
      inputStandardSpeedSetpointFan1 = inputStandardSpeedSetpointFan1 - 10;
      if (inputStandardSpeedSetpointFan1 < 0) inputStandardSpeedSetpointFan1 = 0;
      break;
    case 5:
      gotoPage (2);
      break;
    case 6:
      previousMillisDisplayUpdate = 0;
      StandardSpeedSetpointFan1 = inputStandardSpeedSetpointFan1;
      // Drehzahl Lüfter 1
      eeprom_write_int(2, inputStandardSpeedSetpointFan1);
      tft.setFont(&FreeSans12pt7b);
      tft.setTextColor(colFontColor, colBackColor);
      tft.setCursor(18, 220 + baselineMiddle);
      tft.print (F("WERT IM EEPROM GESPEICHERT"));
      break;
    default:
      previousMillisDisplayUpdate = 0;
      break;
  }
}
// ************************************ ENDE: Page 3  *****************************************************

// ****************************************** Page 4: EINSTELLUNG NORMDREHZAHL L2 *************************
void SetupPage4() {
  inputStandardSpeedSetpointFan2 = StandardSpeedSetpointFan2;
}

void SetupBackgroundPage4() {
  // Übersicht Einstellungen
  tft.setFont(&FreeSans9pt7b);  // Kleiner Font

  tft.fillRect(0, 30, 480 - touchBtnWidth, 20, colWindowTitleBackColor );
  tft.setCursor(160, 30 + baselineSmall);
  tft.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor );
  tft.setTextSize(fontFactorSmall);
  tft.print(F("Normdrehzahl L2"));

  tft.setTextColor(colFontColor, colBackColor );

  tft.setCursor(18, 166 + baselineMiddle);
  tft.print (F("Normdrehzahl Abluftventilator"));

}

void loopDisplayUpdatePage4() {
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colFontColor, colBackColor);
  tft.fillRect(18, 192, 80, numberfieldheight, colBackColor);
  tft.setCursor(18, 192 + baselineMiddle);
  tft.print (inputStandardSpeedSetpointFan2);
}

void NewMenuPage4() {
  //Einstellungen L2
  NewMenuEntry (3, F("+10"));
  NewMenuEntry (4, F("-10"));
  NewMenuEntry (5, F("ESC"));
  NewMenuEntry (6, F("OK"));
}

void DoMenuActionPage4() {
  // L2
  switch (menuBtnPressed) {
    case 3:
      previousMillisDisplayUpdate = 0;
      inputStandardSpeedSetpointFan2 = inputStandardSpeedSetpointFan2 + 10;
      if (inputStandardSpeedSetpointFan2 > 10000) inputStandardSpeedSetpointFan2 = 10000;
      break;
    case 4:
      previousMillisDisplayUpdate = 0;
      inputStandardSpeedSetpointFan2 = inputStandardSpeedSetpointFan2 - 10;
      if (inputStandardSpeedSetpointFan2 < 0) inputStandardSpeedSetpointFan2 = 0;
      break;
    case 5:
      gotoPage (2);
      break;
    case 6:
      previousMillisDisplayUpdate = 0;
      StandardSpeedSetpointFan2 = inputStandardSpeedSetpointFan2;
      // Drehzahl Lüfter 1
      eeprom_write_int(4, inputStandardSpeedSetpointFan2);
      tft.setFont(&FreeSans12pt7b);
      tft.setTextColor(colFontColor, colBackColor);
      tft.setCursor(18, 220 + baselineMiddle);
      tft.print (F("WERT IM EEPROM GESPEICHERT"));
      break;
    default:
      previousMillisDisplayUpdate = 0;
      break;
  }
}
// ************************************ ENDE: Page 4  *****************************************************

// ****************************************** Page 5: KALIBRIERUNG LÜFTER *********************************

void NewMenuPage5() {
  //Einstellungen CAL
  NewMenuEntry (5, F("ESC"));
  NewMenuEntry (6, F("OK"));
}

void DoMenuActionPage5() {
  // CAL
  switch (menuBtnPressed) {
    case 5:
      gotoPage (2);
      break;
    case 6:
      previousMillisDisplayUpdate = 0;
      Serial.println(F("Kalibrierung Lüfter wird gestartet"));
      SpeedCalibrationStart();
      tft.setFont(&FreeSans12pt7b);
      tft.setTextColor(colFontColor, colBackColor);
      tft.setCursor(18, 220 + baselineMiddle);
      tft.print (F("Kalibrierung Lüfter wird gestartet"));
      break;
    default:
      previousMillisDisplayUpdate = 0;
      break;
  }
}
// ************************************ ENDE: Page 5  *****************************************************

// ****************************************** Page 6: WERKSEINSTELLUNGEN **********************************
void NewMenuPage6() {
  //Factory Reset
  NewMenuEntry (5, F("ESC"));
  NewMenuEntry (6, F("OK"));
}

void DoMenuActionPage6() {
  // RESET
  Serial.println ("DoMenuActionPage6");
  Serial.println (menuBtnPressed);
  switch (menuBtnPressed) {
    case 5:
      gotoPage (2);
      break;
    case 6:
      previousMillisDisplayUpdate = 0;
      Serial.println(F("Speicherbereich wird gelöscht"));
      tft.setFont(&FreeSans12pt7b);
      tft.setTextColor(colFontColor, colBackColor);
      tft.setCursor(18, 220 + baselineMiddle);
      tft.println(F("Speicherbereich wird gelöscht"));
      Serial.println ("MP 6, MB 6");
      initializeEEPROM(true);
      // Reboot
      Serial.println(F("Reboot"));
      delay (1000);
      asm volatile ("jmp 0");
      break;
    default:
      previousMillisDisplayUpdate = 0;
      break;
  }
}
// ************************************ ENDE: Page 6  *****************************************************


void loopDisplayUpdate() {
  // Das Update wird alle 1000mS durchlaufen
  // Bevor Werte ausgegeben werden, wird auf Änderungen der Werte überprüft, nur geänderte Werte werden auf das Display geschrieben

  currentMillis = millis();

  if ((currentMillis - previousMillisDisplayUpdate >= intervalDisplayUpdate) || updateDisplayNow) {
    if (serialDebugDisplay == 1) {
      Serial.println("loopDisplayUpdate");
    }

    char strPrint[10];

    // Netzwerkverbindung anzeigen
    if (!bLanOk) {
      if (LastLanOk != bLanOk || updateDisplayNow) {
        LastLanOk = bLanOk;
        tft.fillRect(10, 0, 120, 20, colErrorBackColor);
        tft.setTextColor(colErrorFontColor );
        tft.setCursor(20, 0 + baselineMiddle);
        tft.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft.print(F("ERR LAN"));
      }
    } else if (!bMqttOk) {
      if (LastMqttOk != bMqttOk || updateDisplayNow) {
        LastMqttOk = bMqttOk;
        tft.fillRect(10, 0, 120, 20, colErrorBackColor);
        tft.setTextColor(colErrorFontColor );
        tft.setCursor(20, 0 + baselineMiddle);
        tft.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft.print(F("ERR MQTT"));
      }
    } else {
      LastMqttOk = true;
      LastLanOk = true;
      tft.fillRect(10, 0, 120, 20, colBackColor);
    }

    // Einzelseiten
    switch (menupage) {
      case 0:
        // Standardseite
        loopDisplayUpdatePage0();
        break;
      case 3:
        loopDisplayUpdatePage3();
        break;
      case 4:
        loopDisplayUpdatePage4();
        break;
    }

    Serial.println(ErrorText);
    if (ErrorText.length() > 0 ) {
      if (ErrorText != LastErrorText || updateDisplayNow) {
        // Neuer Fehler
        tft.fillRect(0, 300, 480, 21, colErrorBackColor );
        tft.setTextColor(colErrorFontColor );
        tft.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft.setCursor(18, 301 + baselineSmall);
        tft.print(ErrorText);
        LastErrorText = ErrorText;
        tft.setFont(&FreeSans12pt7b);  // Mittlerer Font
      }
    } else if (InfoText.length() > 0 ) {
      if (InfoText != LastInfoText || updateDisplayNow) {
        // Neuer Fehler
        tft.fillRect(0, 300, 480, 21, colInfoBackColor );
        tft.setTextColor(colInfoFontColor );
        tft.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft.setCursor(18, 301 + baselineSmall);
        tft.print(InfoText);
        LastInfoText = InfoText;
        tft.setFont(&FreeSans12pt7b);  // Mittlerer Font
      }
    }
    else if (ErrorText.length() == 0 && InfoText.length() == 0) {
      tft.fillRect(0, 300, 480, 20, colBackColor );
      LastErrorText = "";
      LastInfoText = "";
    }
    updateDisplayNow = false;
    previousMillisDisplayUpdate = currentMillis;
  }
}


void SetupBackgroundPage() {

  if (serialDebugDisplay == 1) {
    Serial.println(F("SetupBackgroundPage"));
  }
  tft.fillRect(0, 30, 480 - touchBtnWidth, 290, colBackColor);

  switch (menupage) {
    case 0:
      // Standardseite
      SetupBackgroundPage0();
      break;
    case 1:
      // Weitere Sensorwerte
      SetupBackgroundPage1();
      break;
    case 2:
      // Übersicht Einstellungen
      SetupBackgroundPage2();
      break;
    case 3:
      // Übersicht Einstellungen
      SetupPage3();
      SetupBackgroundPage3();
      break;
    case 4:
      // Übersicht Einstellungen
      SetupPage4();
      SetupBackgroundPage4();
      break;
  }
  ShowMenu();
  updateDisplayNow = true;
}

void SetCursor(int x, int y) {
  tft.setCursor(x, y + baselineSmall);
}


// ********************************** Menüfunktionen ******************************************
// menupage = Angezeigte Bildschirmseite
// menuBtnActive[0..6] = true, wenn Menüeintrag sichtbar und aktiviert ist, ansonsten false

void gotoPage(byte PageNo) {
  previousMillisDisplayUpdate = 0;
  menupage = PageNo;
  SetupBackgroundPage();
}

void ShowMenu() {

  // Menu Hintergrund
  tft.fillRect(480 - touchBtnWidth , touchBtnYOffset, touchBtnWidth , 280, colMenuBackColor );

  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colMenuFontColor, colMenuBackColor);

  for (int i = 0; i <= 6; i++) menuBtnActive[i] = false;

  if (menupage == 0) {
    //Standardseite
    NewMenuPage0();
  } else if (menupage == 1) {
    //DHT, CO2, VOC Messwerte
    NewMenuPage1();
  } else if (menupage == 2) {
    //Einstellungen Übersicht
    NewMenuPage2();
  } else if (menupage == 3) {
    //Einstellungen L1
    NewMenuPage3();
  } else if (menupage == 4) {
    //Einstellungen L2
    NewMenuPage4();
  } else if (menupage == 5) {
    //Einstellungen CAL
    NewMenuPage5();
  } else if (menupage == 6) {
    //Factory Reset
    NewMenuPage6();
  }
}


void DoMenuAction() {
  Serial.print (F("DoMenuAction "));
  Serial.println(menupage);

  if (menupage == 0) {
    DoMenuActionPage0();
  }
  if (menupage == 1) {
    DoMenuActionPage1();
  }
  if (menupage == 2) {
    DoMenuActionPage2();
  }
  if (menupage == 3) {
    DoMenuActionPage3();
  }
  if (menupage == 4) {
    DoMenuActionPage4();
  }
  if (menupage == 5) {
    DoMenuActionPage5();
  }
  if (menupage == 6) {
    DoMenuActionPage6();
  }
}

void NewMenuEntry(byte mnuBtn, String mnuTxt) {
  menuBtnActive[mnuBtn] = true;
  PrintMenuBtn (mnuBtn, mnuTxt, colMenuBtnFrame);
}

void PrintMenuBtn (byte mnuBtn, String mnuTxt, long colFrame) {
  // ohne mnuText wird vom Button nur der Rand gezeichnet

  if (mnuBtn > 0 && mnuBtn <= 6 && menuBtnActive[mnuBtn]) {
    int x, y;
    char strPrint[10];
    x = 480 - touchBtnWidth + 1;
    y = touchBtnYOffset + 1 + touchBtnHeight * (mnuBtn - 1);

    if (colFrame == colMenuBtnFrameHL) {
      // Highlight Frame
      tft.drawRoundRect(x, y, touchBtnWidth - 2, touchBtnHeight - 2, 5, colFrame);
      tft.drawRoundRect(x + 1, y + 1, touchBtnWidth - 4, touchBtnHeight - 4, 5, colFrame);
      tft.drawRoundRect(x + 2, y + 2, touchBtnWidth - 6, touchBtnHeight - 6, 5, colFrame);
    } else {
      tft.drawRoundRect(x, y, touchBtnWidth - 2, touchBtnHeight - 2, 5, colFrame);
      tft.drawRoundRect(x + 1, y + 1, touchBtnWidth - 4, touchBtnHeight - 4, 5, colMenuBackColor);
      tft.drawRoundRect(x + 2, y + 2, touchBtnWidth - 6, touchBtnHeight - 6, 5, colMenuBackColor);
    }

    mnuTxt.toCharArray(strPrint, 10) ;
    tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(480 - touchBtnWidth / 2 - w / 2 , y + 15 + baselineSmall);
    tft.print(strPrint);
  }
}

void SetMenuBorder(byte menuBtn) {
  if (millis() - millisLastMenuBtnPress > intervalMenuBtn - 100 || menuBtn > 0) {
    if (menuBtn > 0) {
      PrintMenuBtn (menuBtn, "", colMenuBtnFrameHL);
      LastMenuBtn = menuBtn;
    } else {
      PrintMenuBtn (LastMenuBtn, "", colMenuBtnFrame);
    }
  }
}

void loopTouch()
{
  SetMenuBorder(0);

  if (millis() - millisLastMenuBtnPress > intervalMenuBtn) {

    uint16_t xpos, ypos;  //screen coordinates
    tp = ts.getPoint();   //tp.x, tp.y are ADC values

    // if sharing pins, you'll need to fix the directions of the touchscreen pins
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
    pinMode(XP, OUTPUT);
    pinMode(YM, OUTPUT);
    //    digitalWrite(XM, HIGH);
    //    digitalWrite(YP, HIGH);
    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!

    if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
      // is controller wired for Landscape ? or are we oriented in Landscape?
      if (SwapXY != (Orientation & 1)) SWAP(tp.x, tp.y);
      // scale from 0->1023 to tft.width  i.e. left = 0, rt = width
      // most mcufriend have touch (with icons) that extends below the TFT
      // screens without icons need to reserve a space for "erase"
      // scale the ADC values from ts.getPoint() to screen values e.g. 0-239
      xpos = map(tp.x, TS_LEFT, TS_RT, 0, tft.width());
      ypos = map(tp.y, TS_TOP, TS_BOT, 0, tft.height());

      Serial.print("Touch (xpos / ypos): ");
      Serial.print(xpos);
      Serial.print(" / ");
      Serial.println(ypos);

      // are we in top color box area ?
      if (xpos > 480 - touchBtnWidth) {               //draw white border on selected color box

        if (ypos < touchBtnYOffset + touchBtnHeight * 0) {
          // Headline, Nothing to do
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 1 && menuBtnActive[1]) {
          menuBtnPressed = 1;
          SetMenuBorder(menuBtnPressed);
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 2 && menuBtnActive[2]) {
          menuBtnPressed = 2;
          SetMenuBorder(menuBtnPressed);
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 3 && menuBtnActive[3]) {
          menuBtnPressed = 3;
          SetMenuBorder(menuBtnPressed);
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 4 && menuBtnActive[4]) {
          menuBtnPressed = 4;
          SetMenuBorder(menuBtnPressed);
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 5 && menuBtnActive[5]) {
          menuBtnPressed = 5;
          SetMenuBorder(menuBtnPressed);
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 6 && menuBtnActive[6]) {
          menuBtnPressed = 6;
          SetMenuBorder(menuBtnPressed);
          millisLastMenuBtnPress = millis();
        }
        DoMenuAction();
        menuBtnPressed = -1;
      }
    }
  }
}
// **************************** ENDE  Menüfunktionen ******************************************



// *** TFT starten
void SetupTftScreen() {
  if (serialDebugDisplay == 1) {
    Serial.println("start_tft");
  }
  ID = tft.readID();  // you must detect the correct controller
  tft.begin(ID);      // everything will start working

  int16_t  x1, y1;
  uint16_t w, h;
  tft.setFont(&FreeSansBold24pt7b);  // Großer Font
  // Baseline bestimmen für kleinen Font
  tft.getTextBounds("M", 0, 0, &x1, &y1, &w, &h);
  baselineBigNumber = h;
  Serial.print ("Font baseline (big/middle/small): ");
  Serial.print (h);
  Serial.print (" / ");
  tft.setFont(&FreeSans12pt7b);  // Mittlerer Font
  // Baseline bestimmen für kleinen Font
  tft.getTextBounds("9", 0, 0, &x1, &y1, &w, &h);
  baselineMiddle = h;
  numberfieldheight = h + 1 ;
  Serial.print (numberfieldheight);
  Serial.print (" / ");
  tft.setFont(&FreeSans9pt7b);  // Kleiner Font
  // Baseline bestimmen für kleinen Font
  tft.getTextBounds("M", 0, 0, &x1, &y1, &w, &h);
  baselineSmall = h;
  Serial.print (h);
  Serial.println ();

  Serial.print("TFT controller: ");
  Serial.println(ID);
  tft.setRotation(1);
  tft.fillScreen(colBackColor);
}

void SetupTouch(void)
{
  uint16_t tmp;
  identifier = tft.readID();
  switch (Orientation) {      // adjust for different aspects
    case 0:   break;        //no change,  calibrated for PORTRAIT
    case 1:   tmp = TS_LEFT, TS_LEFT = TS_BOT, TS_BOT = TS_RT, TS_RT = TS_TOP, TS_TOP = tmp;  break;
    case 2:   SWAP(TS_LEFT, TS_RT);  SWAP(TS_TOP, TS_BOT); break;
    case 3:   tmp = TS_LEFT, TS_LEFT = TS_TOP, TS_TOP = TS_RT, TS_RT = TS_BOT, TS_BOT = tmp;  break;
  }
  ts = TouchScreen(XP, YP, XM, YM, 300);     //call the constructor AGAIN with new values.
  show_Touch_Serial();
}

void show_Touch_Serial(void)
{
  Serial.print(F("Found "));
  Serial.print(name);
  Serial.println(F(" LCD driver"));
  Serial.print(F("ID=0x"));
  Serial.println(identifier, HEX);
  Serial.println("Screen is " + String(tft.width()) + "x" + String(tft.height()));
  Serial.println("Calibration is: ");
  Serial.println("LEFT = " + String(TS_LEFT) + " RT  = " + String(TS_RT));
  Serial.println("TOP  = " + String(TS_TOP)  + " BOT = " + String(TS_BOT));
  Serial.print("Wiring is: ");
  Serial.println(SwapXY ? "SWAPXY" : "PORTRAIT");
  Serial.println("YP=" + String(YP)  + " XM=" + String(XM));
  Serial.println("YM=" + String(YM)  + " XP=" + String(XP));
}

// *** oberer Rand
void print_header() {
  tft.fillRect(0, 0, 480, 20, colBackColor);
  tft.setCursor(140, 0 + baselineSmall);
  tft.setTextColor(colFontColor);
  tft.setTextSize(fontFactorSmall);
  tft.print(F(" * Pluggit AP 300 * "));
  tft.setCursor(420, 0 + baselineSmall);
  tft.print(strVersion);

}


