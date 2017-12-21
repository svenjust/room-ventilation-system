unsigned long previousMillisDisplayUpdate = 0;
unsigned long intervalDisplayUpdate = 5000;
boolean updateDisplayNow = false;
int LastDisplaySpeedTachoFan1 = 0;
int LastDisplaySpeedTachoFan2 = 0;
int LastDisplaykwlMode = 0;
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

    if (LastDisplaykwlMode != kwlMode) {
      // KWL Mode
      tft.setCursor(180, 100);
      tft.setTextColor(RED, BLACK);
      tft.setTextSize(2);
      sprintf(strPrint, "%-1i", (int)kwlMode);
      tft.print(strPrint);
      LastDisplaykwlMode = kwlMode;
    }

    // Speed Fan1
    if (abs(LastDisplaySpeedTachoFan1 - speedTachoFan1) > 10) {
      tft.setCursor(30, 100);
      tft.setTextColor(YELLOW, BLACK);
      tft.setTextSize(5);
      sprintf(strPrint, "%-5i", (int)speedTachoFan1);
      tft.print(strPrint);
      LastDisplaySpeedTachoFan1 = speedTachoFan1;
    }

    // Speed Fan2
    if (abs(LastDisplaySpeedTachoFan2 - speedTachoFan2) > 10) {
      tft.setCursor(260, 100);
      tft.setTextColor(YELLOW, BLACK);
      tft.setTextSize(5);
      sprintf(strPrint, "%-5i", (int)speedTachoFan2);
      tft.print(strPrint);
      LastDisplaySpeedTachoFan2 = speedTachoFan2;
    }

    if ((abs(LastDisplayT1 - TEMP1_Aussenluft) > 0.5)
        || (abs(LastDisplayT2 - TEMP2_Zuluft) > 0.5)
        || (abs(LastDisplayT3 - TEMP3_Abluft) > 0.5)
        || (abs(LastDisplayT4 - TEMP4_Fortluft) > 0.5))
    {
      LastDisplayT1 = TEMP1_Aussenluft;
      LastDisplayT2 = TEMP2_Zuluft;
      LastDisplayT3 = TEMP3_Abluft;
      LastDisplayT4 = TEMP4_Fortluft;
      
      tft.setTextColor(YELLOW, BLACK);
      tft.setTextSize(2);

      // T1
      tft.setCursor(18, 225);
      tft.print(TEMP1_Aussenluft, 1);
      tft.println("\xF7");

      // T2
      tft.setCursor(280, 275);
      tft.print(TEMP2_Zuluft, 1);
      tft.print("\xF7");

      // T3
      tft.setCursor(18, 275);
      tft.print(TEMP3_Abluft, 1);
      tft.print("\xF7");

      // T4
      tft.setCursor(280, 225);
      tft.print(TEMP4_Fortluft, 1);
      tft.print("\xF7");
    }
  }
}

void tft_print_background() {

  if (serialDebugDisplay == 1) {
    Serial.println("tft_print_background");
  }
  tft.fillRect(0, 30, 480, 200, BLACK);

  tft.setCursor(180, 30);
  tft.setTextColor(RED, BLACK);
  tft.setTextSize(2);
  tft.print("Stufe: ");

  tft.setCursor(30, 70);
  tft.setTextColor(YELLOW, BLACK);
  tft.setTextSize(2);
  tft.print("Zuluft");

  tft.setCursor(260, 70);
  tft.print("Abluft");

  tft.setCursor(18, 200);
  tft.setTextColor (CYAN, BLACK);
  tft.print("Aussenluft: ");

  tft.setCursor(280, 250);
  tft.print("Zuluft: ");

  tft.setCursor(18, 250);
  tft.print("Abluft: ");

  tft.setCursor(280, 200);
  tft.print("Fortluft: ");

}

// *** TFT starten
void start_tft() {
  if (serialDebugDisplay == 1) {
    Serial.println("start_tft");
  }

  ID = tft.readID();  // you must detect the correct controller
  tft.begin(ID);      // everything will start working
  Serial.print("TFT controller: ");
  Serial.println(ID);
  tft.setRotation(1);
  tft.fillScreen(BLACK);
}
// *** TFT starten

// *** oberer Rand
void print_header() {
  tft.fillRect(0, 0, 480, 20, RED);
  tft.setCursor(125, 0);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print(" * Pluggit AP 300 * ");
}
// *** oberer Rand

// *** unterer Rand
void print_footer() {
  tft.fillRect(0, 300, 480, 20, RED);
  tft.setCursor(130, 300);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print(strVersion);
}
// *** unterer Rand


