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

/*
  // Schwarz auf weiss
  #define colBackColor                0xFFFFFF //  45 127 151
  #define colWindowTitleBackColor     0x000000 //  66 182 218
  #define colWindowTitleFontColor     0xFFFFFF //  66 182 218
  #define colFontColor                0x000000 // 255 255 255
*/

uint16_t identifier;
uint8_t Orientation = 3;    //PORTRAIT

#define fontFactorSmall     1
#define fontFactorBigNumber 3
int     baselineSmall     = 0;
int     baselineMiddle    = 0;
int     baselineBigNumber = 0;
int     numberfieldheight = 0;

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
byte       LastLanOk                   = true;
byte       LastMqttOk                  = true;

byte touchBtnWidth  = 60;
byte touchBtnHeight = 45;
byte touchBtnYOffset = 30;


int16_t  x1, y1;
uint16_t w, h;

void start_touch(void)
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

void loopDisplayUpdate() {
  // Das Update wird alle 1000mS durchlaufen
  // Bevor Werte ausgegeben werden, wird auf Änderungen der Werte überprüft, nur geänderte Werte werden auf das Display geschrieben

  currentMillis = millis();

  if (abs(LastDisplaySpeedTachoFan2 - speedTachoFan2) > 10) {
    updateDisplayNow = true;
  }
  if ((currentMillis - previousMillisDisplayUpdate >= intervalDisplayUpdate) || updateDisplayNow) {
    if (serialDebugDisplay == 1) {
      Serial.println("loopDisplayUpdate");
    }

    char strPrint[10];
    updateDisplayNow = false;
    previousMillisDisplayUpdate = currentMillis;

    // Netzwerkverbindung anzeigen
    if (!bLanOk) {
      if (LastLanOk != bLanOk) {
        LastLanOk = bLanOk;
        tft.fillRect(10, 0, 120, 20, colErrorBackColor);
        tft.setTextColor(colErrorFontColor );
        tft.setCursor(20, 0 + baselineMiddle);
        tft.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft.print("ERR LAN");
      }
    } else if (!bMqttOk) {
      if (LastMqttOk != bMqttOk ) {
        LastMqttOk = bMqttOk;
        tft.fillRect(10, 0, 120, 20, colErrorBackColor);
        tft.setTextColor(colErrorFontColor );
        tft.setCursor(20, 0 + baselineMiddle);
        tft.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft.print("ERR MQTT");
      }
    } else {
      LastMqttOk = true;
      LastLanOk = true;
      tft.fillRect(10, 0, 120, 20, colBackColor);
    }

    if (LastDisplaykwlMode != kwlMode) {
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
    if (abs(LastDisplaySpeedTachoFan1 - speedTachoFan1) > 10) {
      LastDisplaySpeedTachoFan1 = speedTachoFan1;
      tft.fillRect(280, 192, 80, numberfieldheight, colBackColor);
      sprintf(strPrint, "%5i", (int)speedTachoFan1);
      tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(340 - w, 192 + baselineMiddle);
      tft.print(strPrint);
    }
    // Speed Fan2
    if (abs(LastDisplaySpeedTachoFan2 - speedTachoFan2) > 10) {
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
    if (abs(LastDisplayT1 - TEMP1_Aussenluft) > 0.5) {
      LastDisplayT1 = TEMP1_Aussenluft;
      tft.fillRect(160, 166, 80, numberfieldheight, colBackColor);
      sprintf(strPrint, "%5d.%1d", (int)TEMP1_Aussenluft, abs(((int)(TEMP1_Aussenluft * 10)) % 10));
      tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(240 - w, 166 + baselineMiddle);
      tft.print(strPrint);
    }
    // T2
    if (abs(LastDisplayT2 - TEMP2_Zuluft) > 0.5) {
      LastDisplayT2 = TEMP2_Zuluft;
      tft.fillRect(160, 192, 80, numberfieldheight, colBackColor);
      sprintf(strPrint, "%5d.%1d", (int)TEMP2_Zuluft, abs(((int)(TEMP2_Zuluft * 10)) % 10));
      tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(240 - w, 192 + baselineMiddle);
      tft.print(strPrint);
    }
    // T3
    if (abs(LastDisplayT3 - TEMP3_Abluft) > 0.5) {
      LastDisplayT3 = TEMP3_Abluft;
      tft.fillRect(160, 218, 80, numberfieldheight, colBackColor);
      sprintf(strPrint, "%5d.%1d", (int)TEMP3_Abluft, abs(((int)(TEMP3_Abluft * 10)) % 10));
      tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(240 - w, 218 + baselineMiddle);
      tft.print(strPrint);
    }
    // T4
    if (abs(LastDisplayT4 - TEMP4_Fortluft) > 0.5) {
      LastDisplayT4 = TEMP4_Fortluft;
      tft.fillRect(160, 244, 80, numberfieldheight, colBackColor);
      sprintf(strPrint, "%5d.%1d", (int)TEMP4_Fortluft, abs(((int)(TEMP4_Fortluft * 10)) % 10));
      tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(240 - w, 244 + baselineMiddle);
      tft.print(strPrint);
    }
    // Etha Wirkungsgrad
    if (abs(LastEffiencyKwl - EffiencyKwl) > 1) {
      LastEffiencyKwl = EffiencyKwl;
      tft.fillRect(160, 270, 80, numberfieldheight, colBackColor);
      sprintf(strPrint, "%5d %%", (int)EffiencyKwl);
      tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(240 - w, 270 + baselineMiddle);
      tft.print(strPrint);
    }
    Serial.println(ErrorText);
    if (ErrorText.length() > 0 ) {
      if (ErrorText != LastErrorText) {
        // Neuer Fehler
        tft.fillRect(0, 300, 479, 21, colErrorBackColor );
        tft.setTextColor(colErrorFontColor );
        tft.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft.setCursor(18, 301 + baselineSmall);
        tft.print(ErrorText);
        LastErrorText = ErrorText;
        tft.setFont(&FreeSans12pt7b);  // Mittlerer Font
      }
    } else if (InfoText.length() > 0 ) {
      if (InfoText != LastInfoText) {
        // Neuer Fehler
        tft.fillRect(0, 300, 479, 21, colInfoBackColor );
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
  }
}

void tft_print_background() {

  if (serialDebugDisplay == 1) {
    Serial.println("tft_print_background");
  }
  tft.fillRect(0, 30, 480, 290, colBackColor);

  tft.fillRect(0, 30, 480, 20, colWindowTitleBackColor );

  tft.setCursor(160, 30 + baselineSmall);
  tft.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor );
  tft.setTextSize(fontFactorSmall);
  tft.print("Lueftungsstufe");

  tft.setTextColor(colFontColor, colBackColor );
  tft.setCursor(150, 140 + baselineSmall);
  tft.print("Temperatur");

  tft.setCursor(270, 140 + baselineSmall);
  tft.print("Luefterdrehzahl");


  tft.setCursor(18, 166 + baselineMiddle);
  tft.print("Aussenluft");

  tft.setCursor(18, 192 + baselineMiddle);
  tft.print("Zuluft");

  tft.setCursor(18, 218 + baselineMiddle);
  tft.print("Abluft");

  tft.setCursor(18, 244 + baselineMiddle);
  tft.print("Fortluft");

  tft.setCursor(18, 270 + baselineMiddle);
  tft.print("Wirkungsgrad");

  tft_print_menu();
}


void SetCursor(int x, int y) {
  tft.setCursor(x, y + baselineSmall);
}


byte          menupage                 = 0;
byte          menuBtnPressed           = -1;
unsigned long millisLastMenuBtnPress   = 0;
unsigned long intervalMenuBtn          = 500;

void tft_print_menu() {
  
  // Menu Hintergrund
  tft.fillRect(480 - touchBtnWidth , touchBtnYOffset, touchBtnWidth , 280, colMenuBackColor );

  
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colMenuFontColor, colMenuBackColor);

  if (menupage == 0) {
    //
    //6 Tasten auf 320 - 40 = 280 Aussenhöhe, 1 Pixel leer
    // Breite 45 außen
    //tft_print_menu_btn (1, "1");
    //tft_print_menu_btn (2, "2");
    tft_print_menu_btn (3, "+");
    tft_print_menu_btn (4, "-");
    //tft_print_menu_btn (5, "ESC");
    //tft_print_menu_btn (6, "OK");
  }
}

void tft_print_menu_btn (byte mnuBtn, String mnuTxt) {
  int x, y;
  char strPrint[10];
  x = 480 - touchBtnWidth + 1;
  y = touchBtnYOffset + 1 + touchBtnHeight * (mnuBtn - 1);

  tft.drawRoundRect(x, y, touchBtnWidth - 2, touchBtnHeight - 2, 5, colMenuBtnFrame);
  mnuTxt.toCharArray(strPrint, 10) ;
  tft.getTextBounds(strPrint, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(480 - touchBtnWidth/2 - w/2 , y + 15 + baselineSmall);
  tft.print(strPrint);
}

void loopTouch()
{
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
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 1) {
          menuBtnPressed = 1;
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 2) {
          menuBtnPressed = 2;
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 3) {
          menuBtnPressed = 3;
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 4) {
          menuBtnPressed = 4;
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 5) {
          menuBtnPressed = 5;
          millisLastMenuBtnPress = millis();
        } else if (ypos <  touchBtnYOffset + touchBtnHeight * 6) {
          menuBtnPressed = 6;
          millisLastMenuBtnPress = millis();
        }
        DoMenuAction();
      }
    }
  }
}

void DoMenuAction() {

  if (menupage == 0) {
    // Standardseite
    switch (menuBtnPressed) {
      case 1:
        previousMillisDisplayUpdate = 0;
        Serial.println ("MP 0, MB 1");
        break;
      case 2:
        previousMillisDisplayUpdate = 0;
        Serial.println ("MP 0, MB 2");
        break;
      case 3:
        if (kwlMode < defStandardModeCnt - 1)  kwlMode = kwlMode + 1;
        previousMillisDisplayUpdate = 0;
        Serial.println ("MP 0, MB 3");
        break;
      case 4:
        if (kwlMode > 0)  kwlMode = kwlMode - 1;
        previousMillisDisplayUpdate = 0;
        Serial.println ("MP 0, MB 4");
        break;
      case 5:
        previousMillisDisplayUpdate = 0;
        Serial.println ("MP 0, MB 5");
        break;
      case 6:
        previousMillisDisplayUpdate = 0;
        Serial.println ("MP 0, MB 6");
        break;
    }
  }
  menuBtnPressed = -1;
}



// *** TFT starten
void start_tft() {
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
// *** TFT starten

// *** oberer Rand
void print_header() {
  tft.fillRect(0, 0, 480, 20, colBackColor);
  tft.setCursor(140, 0 + baselineSmall);
  tft.setTextColor(colFontColor);
  tft.setTextSize(fontFactorSmall);
  tft.print(" * Pluggit AP 300 * ");
  tft.setCursor(420, 0 + baselineSmall);
  tft.print(strVersion);

}
// *** oberer Rand

// *** unterer Rand
void print_footer() {
  /*
    tft.fillRect(0, 300, 480, 20, colBackColor);
    tft.setCursor(130, 300 + baselineSmall);
    tft.setTextColor(colFontColor);
    tft.setTextSize(fontFactorSmall);
    tft.print(strVersion);
  */
}
// *** unterer Rand


