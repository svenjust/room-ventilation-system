

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

