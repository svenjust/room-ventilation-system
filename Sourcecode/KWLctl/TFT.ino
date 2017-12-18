unsigned long previousMillisDisplayUpdate = 0;
unsigned long intervalDisplayUpdate = 500;

void loopDisplayUpdate() {
  currentMillis = millis();
  if (currentMillis - previousMillisDisplayUpdate >= intervalDisplayUpdate) {
  previousMillisDisplayUpdate = currentMillis;
    tft.setCursor(180, 30);
    tft.setTextColor(RED, BLACK);
    tft.setTextSize(2);
    tft.print("Stufe: ");
    tft.print(kwlMode);

    tft.setCursor(30, 70);
    tft.setTextColor(YELLOW, BLACK);
    tft.setTextSize(2);
    tft.print("Zuluft");
    tft.setCursor(30, 100);
    tft.setTextColor(YELLOW, BLACK);
    tft.setTextSize(5);
    tft.print(speedTachoFan1, 0);
    tft.setTextSize(2);
    //tft.print(" U/min");

    tft.setCursor(260, 70);
    tft.setTextColor(YELLOW, BLACK);
    tft.setTextSize(2);
    tft.print("Abluft");
    tft.setCursor(260, 100);
    tft.setTextColor(YELLOW, BLACK);
    tft.setTextSize(5);
    tft.print(speedTachoFan2, 0);
    tft.setTextSize(2);
    //tft.print(" U/min");

    tft.setCursor(18, 200);
    tft.setTextColor (CYAN, BLACK);
    tft.setTextSize(2);
    tft.print("Aussenluft: ");
    //tft.setCursor(18, 225);
    tft.print(TEMP1_Aussenluft, 1);
    tft.println("\xF7");

    tft.setCursor(280, 250);
    tft.setTextColor (CYAN, BLACK);
    tft.setTextSize(2);
    tft.print("Zuluft: ");
    //tft.setCursor(280, 275);
    tft.print(TEMP2_Zuluft, 1);
    tft.print("\xF7");

    tft.setCursor(18, 250);
    tft.setTextColor (CYAN, BLACK);
    tft.setTextSize(2);
    tft.print("Abluft: ");
    //tft.setCursor(18, 275);
    tft.print(TEMP3_Abluft, 1);
    tft.print("\xF7");

    tft.setCursor(280, 200);
    tft.setTextColor (CYAN, BLACK);
    tft.setTextSize(2);
    tft.print("Fortluft: ");
    //tft.setCursor(280, 225);
    tft.print(TEMP4_Fortluft, 1);
    tft.print("\xF7");
  }
}

// *** TFT starten 
void start_tft() {
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
  tft.print(" Version 1.0 BETA ");
}
// *** unterer Rand


