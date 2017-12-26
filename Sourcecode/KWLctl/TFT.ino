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


// Weiss auf Schwarz
#define colBackColor                0x000000 //  45 127 151
#define colWindowTitleBackColor     0xFFFFFF //  66 182 218
#define colWindowTitleFontColor     0x000000 //  66 182 218
#define colFontColor                0xFFFFFF // 255 255 255
#define colErrorBackColor           0xF800 //
#define colErrorFontColor           0xFFFFFF //

/*
  // Schwarz auf weiss
  #define colBackColor                0xFFFFFF //  45 127 151
  #define colWindowTitleBackColor     0x000000 //  66 182 218
  #define colWindowTitleFontColor     0xFFFFFF //  66 182 218
  #define colFontColor                0x000000 // 255 255 255
*/

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
double LastDisplayT1 = 0, LastDisplayT2 = 0, LastDisplayT3 = 0, LastDisplayT4 = 0;

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
    int16_t  x1, y1;
    uint16_t w, h;

    if (LastDisplaykwlMode != kwlMode) {
      // KWL Mode
      tft.setFont(&FreeSansBold24pt7b);
      tft.setCursor(200, 55 + 2 * baselineBigNumber);
      tft.setTextColor(colFontColor, colBackColor);
      tft.setTextSize(2);
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
      tft.fillRect(280, 192, 60, numberfieldheight, colBackColor);
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
      tft.fillRect(280, 218, 60, numberfieldheight, colBackColor);
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
    }
    else if (ErrorText != LastErrorText) {
      tft.fillRect(0, 300, 480, 20, colBackColor );
      LastErrorText = ErrorText;
    }
  }
}

void tft_print_background() {

  if (serialDebugDisplay == 1) {
    Serial.println("tft_print_background");
  }
  tft.fillRect(0, 30, 480, 200, colBackColor);

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
}

void SetCursor(int x, int y) {
  tft.setCursor(x, y + baselineSmall);
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


