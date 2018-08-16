/*
 * Copyright (C) 2018 Sven Just (sven@familie-just.de)
 * Copyright (C) 2018 Ivan Schréter (schreter@gmx.net)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This copyright notice MUST APPEAR in all copies of the software!
 */

#include "TFT.h"
#include "KWLConfig.h"
#include "KWLControl.hpp"

#include <Adafruit_GFX.h>       // TFT

#include <avr/wdt.h>

// Fonts einbinden
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

/// Minimum acceptable pressure.
static constexpr int16_t MINPRESSURE = 20;
/// Maximum acceptable pressure.
static constexpr int16_t MAXPRESSURE = 1000;

#define SWAP(a, b) {auto tmp = a; a = b; b = tmp;}

/******************************************* Seitenverwaltung ********************************************
  Jeder Screen ist durch screen*() Methode aufgesetzt und muss folgende Aktionen tun:
   - Hintergrund vorbereiten
   - Menü vorbereiten via newMenuEntry() Calls, mit Aktionen (z.B., via gotoScreen() um Screen zu wechseln)
   - Falls Screenupdates gebraucht werden, display_update_ setzen (es wird auch initial aufgerufen)
   - Falls screenspezifische Variablen gebraucht werden, initialisieren (in screen_state_)
*/

// Timing:

/// Interval for updating displayed values (1s).
static constexpr unsigned long INTERVAL_DISPLAY_UPDATE = 5000000;
/// Interval for detecting menu button press (500ms).
static constexpr unsigned long INTERVAL_MENU_BTN = 500;
/// Interval for returning back to main screen if nothing pressed (1m).
static constexpr unsigned long INTERVAL_TOUCH_TIMEOUT = 60000;

// Pseudo-constants initialized at TFT initialization based on font data:

/// Baseline for small font.
static int BASELINE_SMALL      = 0;
/// Baseline for middle font.
static int BASELINE_MIDDLE     = 0;
/// Baseline for big font (fan mode).
static int BASELINE_BIGNUMBER  = 0;
/// Height for number field.
static int HEIGHT_NUMBER_FIELD = 0;

// Pseudo-constants initialized at TFT initialization time based on orientation:

static uint16_t TS_LEFT = KWLConfig::TouchLeft;
static uint16_t TS_RT   = KWLConfig::TouchRight;
static uint16_t TS_TOP  = KWLConfig::TouchTop;
static uint16_t TS_BOT  = KWLConfig::TouchBottom;


// FARBEN:

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

/// Menu button width.
static constexpr byte TOUCH_BTN_WIDTH = 60;
/// Menu button height.
static constexpr byte TOUCH_BTN_HEIGHT = 45;
/// First menu button Y offset.
static constexpr byte TOUCH_BTN_YOFFSET = 30;


template<typename Func>
TFT::MenuAction& TFT::MenuAction::operator=(Func&& f) noexcept
{
  using FType = typename scheduler_cpp11_support::remove_reference<Func>::type;
  static_assert(sizeof(FType) <= sizeof(state_), "Too big function closure");
  new(state_) FType(f);
  invoker_ = [](MenuAction* m)
  {
    void* pfunc = m->state_;
    (*reinterpret_cast<FType*>(pfunc))();
  };
  return *this;
}

TFT::TFT() noexcept :
  // For better pressure precision, we need to know the resistance
  // between X+ and X- Use any multimeter to read it
  // For the one we're using, its 300 ohms across the X plate
  ts_(KWLConfig::XP, KWLConfig::YP, KWLConfig::XM, KWLConfig::YM, 300),
  display_update_stats_(F("DisplayUpdate")),
  display_update_task_(display_update_stats_, &TFT::displayUpdate, *this),
  process_touch_stats_(F("ProcessTouch")),
  process_touch_task_(process_touch_stats_, &TFT::loopTouch, *this)
{}

void TFT::begin(Print& /*initTracer*/, KWLControl& control) noexcept {
  control_ = &control;
  gotoScreen(&TFT::screenMain);   // Bootmeldungen löschen, Hintergrund für Standardanzeige starten
}

// ****************************************** Screen 0: HAUPTSEITE ******************************************
void TFT::screenMain() noexcept
{
  // Menu Screen 0 Standardseite

  char title[] = "Lueftungsstufe";
  printScreenTitle(title);

  tft_.setCursor(150, 140 + BASELINE_SMALL);
  tft_.print(F("Temperatur"));

  tft_.setCursor(270, 140 + BASELINE_SMALL);
  tft_.print(F("Luefterdrehzahl"));

  tft_.setCursor(18, 166 + BASELINE_MIDDLE);
  tft_.print(F("Aussenluft"));

  tft_.setCursor(18, 192 + BASELINE_MIDDLE);
  tft_.print(F("Zuluft"));

  tft_.setCursor(18, 218 + BASELINE_MIDDLE);
  tft_.print(F("Abluft"));

  tft_.setCursor(18, 244 + BASELINE_MIDDLE);
  tft_.print(F("Fortluft"));

  tft_.setCursor(18, 270 + BASELINE_MIDDLE);
  tft_.print(F("Wirkungsgrad"));

  display_update_ = &TFT::displayUpdateMain;
  screen_state_.main_.kwl_mode_ = -1;
  screen_state_.main_.tacho_fan1_ = -1;
  screen_state_.main_.tacho_fan2_ = -1;
  screen_state_.main_.t1_ = screen_state_.main_.t2_ =
      screen_state_.main_.t3_ = screen_state_.main_.t4_ = -1000.0;  // some invalid value
  screen_state_.main_.efficiency_ = -1;

  newMenuEntry(1, F("->"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(2, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenAdditionalSensors);
    }
  );
  newMenuEntry(3, F("+"),
    [this]() noexcept {
      if (control_->getFanControl().getVentilationMode() < int(KWLConfig::StandardModeCnt - 1)) {
        control_->getFanControl().setVentilationMode(control_->getFanControl().getVentilationMode() + 1);
        displayUpdateMain();
      }
    }
  );
  newMenuEntry(4, F("-"),
    [this]() noexcept {
      if (control_->getFanControl().getVentilationMode() > 0) {
        control_->getFanControl().setVentilationMode(control_->getFanControl().getVentilationMode() - 1);
        displayUpdateMain();
      }
    }
  );

  // Ende Menu Screen 1
}

void TFT::displayUpdateMain() noexcept {
  // Menu Screen 0
  char buffer[8];
  auto currentMode = control_->getFanControl().getVentilationMode();
  if (screen_state_.main_.kwl_mode_ != currentMode) {
    // KWL Mode
    tft_.setFont(&FreeSansBold24pt7b);
    tft_.setTextColor(colFontColor, colBackColor);
    tft_.setTextSize(2);

    tft_.setCursor(200, 55 + 2 * BASELINE_BIGNUMBER);
    snprintf(buffer, sizeof(buffer), "%-1i", currentMode);
    tft_.fillRect(200, 55, 60, 80, colBackColor);
    tft_.print(buffer);
    screen_state_.main_.kwl_mode_ = currentMode;
    tft_.setTextSize(1);
  }
  tft_.setFont(&FreeSans12pt7b);
  tft_.setTextColor(colFontColor, colBackColor);

  int16_t  x1, y1;
  uint16_t w, h;

  // Speed Fan1
  int speed1 = control_->getFanControl().getFan1().getSpeed();
  if (abs(screen_state_.main_.tacho_fan1_ - speed1) > 10) {
    screen_state_.main_.tacho_fan1_ = speed1;
    tft_.fillRect(280, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    snprintf(buffer, sizeof(buffer), "%5i", speed1);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(340 - w, 192 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // Speed Fan2
  int speed2 = control_->getFanControl().getFan2().getSpeed();
  if (abs(screen_state_.main_.tacho_fan2_ - speed2) > 10) {
    screen_state_.main_.tacho_fan2_ = speed2;
    snprintf(buffer, sizeof(buffer), "%5i", speed2);
    // Debug einkommentieren
    // tft_.fillRect(280, 218, 60, HEIGHT_NUMBER_FIELD, colWindowTitle);
    tft_.fillRect(280, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(340 - w, 218 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T1
  if (abs(screen_state_.main_.t1_ - control_->getTempSensors().get_t1_outside()) > 0.5) {
    screen_state_.main_.t1_ = control_->getTempSensors().get_t1_outside();
    tft_.fillRect(160, 166, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t1_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - w, 166 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T2
  if (abs(screen_state_.main_.t2_ - control_->getTempSensors().get_t2_inlet()) > 0.5) {
    screen_state_.main_.t2_ = control_->getTempSensors().get_t2_inlet();
    tft_.fillRect(160, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t2_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - w, 192 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T3
  if (abs(screen_state_.main_.t3_ - control_->getTempSensors().get_t3_outlet()) > 0.5) {
    screen_state_.main_.t3_ = control_->getTempSensors().get_t3_outlet();
    tft_.fillRect(160, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t3_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - w, 218 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T4
  if (abs(screen_state_.main_.t4_ - control_->getTempSensors().get_t4_exhaust()) > 0.5) {
    screen_state_.main_.t4_ = control_->getTempSensors().get_t4_exhaust();
    tft_.fillRect(160, 244, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t4_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - w, 244 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // Etha Wirkungsgrad
  if (abs(screen_state_.main_.efficiency_ - control_->getTempSensors().getEfficiency()) > 1) {
    screen_state_.main_.efficiency_ = control_->getTempSensors().getEfficiency();
    tft_.fillRect(160, 270, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    snprintf(buffer, sizeof(buffer), "%5d %%", screen_state_.main_.efficiency_);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - w, 270 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
}

// ************************************ ENDE: Screen 0  *****************************************************

// ****************************************** Screen 1: WEITERE SENSORWERTE *********************************
void TFT::screenAdditionalSensors() noexcept
{
  // Menu Screen 1, Sensorwerte

  char title[] = "Weitere Sensorwerte";
  printScreenTitle(title);

  if (control_->getAdditionalSensors().hasDHT1()) {
    tft_.setCursor(18, 166 + BASELINE_MIDDLE);
    tft_.print(F("DHT1"));
  }

  if (control_->getAdditionalSensors().hasDHT2()) {
    tft_.setCursor(18, 192 + BASELINE_MIDDLE);
    tft_.print(F("DHT2"));
  }

  if (control_->getAdditionalSensors().hasCO2()) {
    tft_.setCursor(18, 218 + BASELINE_MIDDLE);
    tft_.print(F("CO2"));
  }
  if (control_->getAdditionalSensors().hasVOC()) {
    tft_.setCursor(18, 244 + BASELINE_MIDDLE);
    tft_.print(F("VOC"));
  }

  display_update_ = &TFT::displayUpdateAdditionalSensors;
  screen_state_.addt_sensors_.dht1_temp_ =
  screen_state_.addt_sensors_.dht2_temp_ = -1000;
  screen_state_.addt_sensors_.mhz14_co2_ppm_ =
  screen_state_.addt_sensors_.tgs2600_voc_ppm_ = INT16_MIN / 2;

  newMenuEntry(1, F("->"),
    [this]() noexcept {
      gotoScreen(&TFT::screenMain);
    }
  );
}

void TFT::displayUpdateAdditionalSensors() noexcept {

  tft_.setFont(&FreeSans12pt7b);
  tft_.setTextColor(colFontColor, colBackColor);

  // TODO what about % humidity?
  char buffer[16];
  int16_t  x1, y1;
  uint16_t w, h;

  // DHT 1
  if (control_->getAdditionalSensors().hasDHT1()) {
    if (abs(screen_state_.addt_sensors_.dht1_temp_ - control_->getAdditionalSensors().getDHT1Temp()) > 0.5) {
      screen_state_.addt_sensors_.dht1_temp_ = control_->getAdditionalSensors().getDHT1Temp();
      tft_.fillRect(160, 166, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(screen_state_.addt_sensors_.dht1_temp_, 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - w, 166 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
  }
  // DHT 2
  if (control_->getAdditionalSensors().hasDHT2()) {
    if (abs(screen_state_.addt_sensors_.dht2_temp_ - control_->getAdditionalSensors().getDHT2Temp()) > 0.5) {
      screen_state_.addt_sensors_.dht2_temp_ = control_->getAdditionalSensors().getDHT2Temp();
      tft_.fillRect(160, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(screen_state_.addt_sensors_.dht2_temp_, 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - w, 192 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
  }

  // CO2
  if (control_->getAdditionalSensors().hasCO2()) {
    if (abs(screen_state_.addt_sensors_.mhz14_co2_ppm_ - control_->getAdditionalSensors().getCO2()) > 10) {
      screen_state_.addt_sensors_.mhz14_co2_ppm_ = control_->getAdditionalSensors().getCO2();
      tft_.fillRect(160, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      snprintf(buffer, sizeof(buffer), "%5d", screen_state_.addt_sensors_.mhz14_co2_ppm_);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - w, 218 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
  }

  // VOC
  if (control_->getAdditionalSensors().hasVOC()) {
    if (abs(screen_state_.addt_sensors_.tgs2600_voc_ppm_ - control_->getAdditionalSensors().getVOC()) > 10) {
      screen_state_.addt_sensors_.tgs2600_voc_ppm_ = control_->getAdditionalSensors().getVOC();
      tft_.fillRect(160, 244, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      snprintf(buffer, sizeof(buffer), "%5d", screen_state_.addt_sensors_.tgs2600_voc_ppm_);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - w, 244 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
  }
}

// ************************************ ENDE: Screen 1  *****************************************************

// ****************************************** Screen 2: EINSTELLUNGEN ÜBERSICHT******************************
void TFT::screenSetup() noexcept {
  // Übersicht Einstellungen
  char title[] = "Einstellungen";
  printScreenTitle(title);

  tft_.setTextColor(colFontColor, colBackColor );

  tft_.setCursor(18, 121 + BASELINE_MIDDLE);
  tft_.print (F("L1:  Normdrehzahl Zuluftventilator einstellen"));
  tft_.setCursor(18, 166 + BASELINE_MIDDLE);
  tft_.print (F("L2:  Normdrehzahl Abluftventilator einstellen"));
  tft_.setCursor(18, 211 + BASELINE_MIDDLE);
  tft_.print(F("KAL: Kalibrierung der Ventilatoransteuerung"));
  tft_.setCursor(18, 256 + BASELINE_MIDDLE);
  tft_.print(F("RGL: Regelung der Ventilatoren"));

  newMenuEntry(1, F("->"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupFactoryDefaults);
    }
  );
  newMenuEntry(2, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenMain);
    }
  );
  newMenuEntry(3, F("L1"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupFan1);
    }
  );
  newMenuEntry(4, F("L2"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupFan2);
    }
  );
  newMenuEntry(5, F("KAL"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupFanCalibration);
    }
  );
  newMenuEntry(6, F("RGL"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupFanMode);
    }
  );
}

// ************************************ ENDE: Screen 2  *****************************************************

// ****************************************** Screen 3/4: EINSTELLUNG NORMDREHZAHL L1/L2 *************************
void TFT::screenSetupFan1() noexcept {
  char title[] = "Normdrehzahl Zuluftventilator";
  printScreenTitle(title);
  screen_state_.fan_.input_standard_speed_setpoint_ = control_->getFanControl().getFan1().getStandardSpeed();
  screenSetupFan(1);
}

void TFT::screenSetupFan2() noexcept {
  char title[] = "Normdrehzahl Abluftventilator";
  printScreenTitle(title);
  screen_state_.fan_.input_standard_speed_setpoint_ = control_->getFanControl().getFan2().getStandardSpeed();
  screenSetupFan(2);
}

void TFT::screenSetupFan(uint8_t fan_idx) noexcept {
  screen_state_.fan_.fan_index_ = fan_idx;
  tft_.setCursor(18, 75 + BASELINE_MIDDLE);
  tft_.print (F("Mit dem Button 'OK' wird der Wert gespeichert."));
  tft_.setCursor(18, 100 + BASELINE_MIDDLE);
  tft_.print (F("Der aktueller Wert betraegt: "));
  tft_.print (int(control_->getFanControl().getFan1().getStandardSpeed()));
  tft_.println(F(" U / min"));
  tft_.setCursor(18, 150 + BASELINE_MIDDLE);
  tft_.print (F("Neuer Wert: "));
  displayUpdateFan();

  newMenuEntry(1, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(3, F("+ 10"),
    [this]() noexcept {
      screen_state_.fan_.input_standard_speed_setpoint_ += 10;
      if (screen_state_.fan_.input_standard_speed_setpoint_ > 10000)
        screen_state_.fan_.input_standard_speed_setpoint_ = 10000;
      displayUpdateFan();
    }
  );
  newMenuEntry(4, F("- 10"),
    [this]() noexcept {
      if (screen_state_.fan_.input_standard_speed_setpoint_ > 10)
        screen_state_.fan_.input_standard_speed_setpoint_ -= 10;
      else
        screen_state_.fan_.input_standard_speed_setpoint_ = 0;
      displayUpdateFan();
    }
  );
  newMenuEntry(6, F("OK"),
    [this]() noexcept {
      // Drehzahl Lüfter 1/2
      if (screen_state_.fan_.fan_index_ == 1) {
        control_->getFanControl().getFan1().setStandardSpeed(screen_state_.fan_.input_standard_speed_setpoint_);
        control_->getPersistentConfig().setSpeedSetpointFan1(screen_state_.fan_.input_standard_speed_setpoint_);
      } else {
        control_->getFanControl().getFan2().setStandardSpeed(screen_state_.fan_.input_standard_speed_setpoint_);
        control_->getPersistentConfig().setSpeedSetpointFan2(screen_state_.fan_.input_standard_speed_setpoint_);
      }
      tft_.setFont(&FreeSans9pt7b);
      tft_.setTextColor(colFontColor, colBackColor);
      tft_.setCursor(18, 225 + BASELINE_MIDDLE);
      tft_.print (F("Wert im EEPROM gespeichert"));
      tft_.setCursor(18, 250 + BASELINE_MIDDLE);
      tft_.print (F("Bitte Kalibrierung starten!"));
    }
  );
}

void TFT::displayUpdateFan() noexcept {
  tft_.setFont(&FreeSans12pt7b);
  tft_.setTextColor(colFontColor, colBackColor);
  tft_.fillRect(18, 175, 80, HEIGHT_NUMBER_FIELD, colBackColor);
  tft_.setCursor(18, 175 + BASELINE_MIDDLE);
  tft_.print(screen_state_.fan_.input_standard_speed_setpoint_);
}

// ************************************ ENDE: Screen 3/4  *****************************************************

// ****************************************** Screen 5: KALIBRIERUNG LÜFTER *********************************
void TFT::screenSetupFanCalibration() noexcept {
  char title[] = "Kalibrierung Ventilatoren";
  printScreenTitle(title);

  tft_.setCursor(18, 75 + BASELINE_MIDDLE);
  tft_.print (F("Bei der Kalibrierung werden die Drehzahlen"));
  tft_.setCursor(18, 100 + BASELINE_MIDDLE);
  tft_.print (F("der Luefter eingestellt und die notwendigen"));
  tft_.setCursor(18, 125 + BASELINE_MIDDLE);
  tft_.print (F("PWM-Werte für jede Stufe gespeichert."));

  newMenuEntry(1, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(6, F("OK"),
    [this]() noexcept {
      control_->getFanControl().speedCalibrationStart();
      tft_.setFont(&FreeSans9pt7b);
      tft_.setTextColor(colFontColor, colBackColor);
      tft_.setCursor(18, 211 + BASELINE_MIDDLE);
      tft_.print (F("Kalibrierung Luefter ist gestartet"));
    }
  );
}

// ************************************ ENDE: Screen 5  *****************************************************

// ****************************************** Screen 6: WERKSEINSTELLUNGEN **********************************
void TFT::screenSetupFactoryDefaults() noexcept
{
  char title[] = "Ruecksetzen auf Werkseinstellungen";
  printScreenTitle(title);

  tft_.setTextColor(colFontColor, colBackColor );
  tft_.setFont(&FreeSans9pt7b);

  tft_.setCursor(18, 125 + BASELINE_MIDDLE);
  tft_.print (F("Es werden alle Werte der Steuerung auf die"));
  tft_.setCursor(18, 150 + BASELINE_MIDDLE);
  tft_.print (F("Standardwerte zurueckgesetzt."));
  tft_.setCursor(18, 175 + BASELINE_MIDDLE);
  tft_.print(F("Die Steuerung wird anschliessend neu gestartet."));

  newMenuEntry(1, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(6, F("OK"),
    [this]() noexcept {
      Serial.println(F("Speicherbereich wird geloescht..."));
      tft_.setFont(&FreeSans9pt7b);
      tft_.setTextColor(colFontColor, colBackColor);
      tft_.setCursor(18, 220 + BASELINE_MIDDLE);
      tft_.println(F("Speicherbereich wird geloescht..."));

      control_->getPersistentConfig().factoryReset();

      tft_.setCursor(18, 250 + BASELINE_MIDDLE);
      tft_.println(F("Loeschung erfolgreich, jetzt Reboot..."));

      // Reboot
      Serial.println(F("Reboot"));
      wdt_disable();
      delay(5000);
      asm volatile ("jmp 0");
    }
  );
}

// ************************************ ENDE: Screen 6  *****************************************************

// ****************************************** Screen 7: REGELUNG VENTILATOREN **********************************

void TFT::screenSetupFanMode() noexcept {
  // Übersicht Einstellungen
  char title[] = "Einstellungen Regelung Ventilatoren";
  printScreenTitle(title);
  screen_state_.fan_calculate_speed_mode_ = control_->getFanControl().getCalculateSpeedMode();

  tft_.setTextColor(colFontColor, colBackColor );

  tft_.setCursor(18, 75 + BASELINE_MIDDLE);
  tft_.print (F("Die Luefter koennen mit festen PWM Werten"));
  tft_.setCursor(18, 100 + BASELINE_MIDDLE);
  tft_.print (F("oder dynamisch per PID-Regler geregelt"));
  tft_.setCursor(18, 125 + BASELINE_MIDDLE);
  tft_.print(F("werden. Aktuelle Einstellung: "));
  if (screen_state_.fan_calculate_speed_mode_ == FanCalculateSpeedMode::PROP) {
    tft_.print(F("PWM Wert"));
  } else {
    tft_.print(F("PID-Regler"));
  }
  tft_.setCursor(18, 175 + BASELINE_MIDDLE);
  tft_.print (F("Neuer Wert: "));
  displayUpdateFanMode();

  newMenuEntry(1, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(3, F("PWM"),
    [this]() noexcept {
      screen_state_.fan_calculate_speed_mode_ = FanCalculateSpeedMode::PROP;
      displayUpdateFanMode();
    }
  );
  newMenuEntry(4, F("PID"),
    [this]() noexcept {
      screen_state_.fan_calculate_speed_mode_ = FanCalculateSpeedMode::PID;
      displayUpdateFanMode();
    }
  );
  newMenuEntry(6, F("OK"),
    [this]() noexcept {
      if ((screen_state_.fan_calculate_speed_mode_ == FanCalculateSpeedMode::PROP || screen_state_.fan_calculate_speed_mode_ == FanCalculateSpeedMode::PID) &&
          screen_state_.fan_calculate_speed_mode_ != control_->getFanControl().getCalculateSpeedMode()) {
        control_->getFanControl().setCalculateSpeedMode(screen_state_.fan_calculate_speed_mode_);
        tft_.setFont(&FreeSans9pt7b);
        tft_.setTextColor(colFontColor, colBackColor);
        tft_.fillRect(18, 250, 200, HEIGHT_NUMBER_FIELD, colBackColor);
        tft_.setCursor(18, 250 + BASELINE_MIDDLE);
        tft_.print(F("Wert gespeichert."));
      } else {
        tft_.setFont(&FreeSans9pt7b);
        tft_.setTextColor(colFontColor, colBackColor);
        tft_.fillRect(18, 250, 200, HEIGHT_NUMBER_FIELD, colBackColor);
        tft_.setCursor(18, 250 + BASELINE_MIDDLE);
        tft_.print(F("Wert nicht geaendert"));
      }
    }
  );
}

void TFT::displayUpdateFanMode() noexcept {
  tft_.setFont(&FreeSans9pt7b);
  tft_.setTextColor(colFontColor, colBackColor);
  tft_.fillRect(18, 200, 130, HEIGHT_NUMBER_FIELD, colBackColor);
  tft_.setCursor(18, 200 + BASELINE_MIDDLE);
  if (screen_state_.fan_calculate_speed_mode_ == FanCalculateSpeedMode::PROP)
    tft_.print(F("PWM Wert"));
  else if (screen_state_.fan_calculate_speed_mode_ == FanCalculateSpeedMode::PID)
    tft_.print(F("PID-Regler"));
  else
    tft_.print(F("???"));
}

// ************************************ ENDE: Screen 7  *****************************************************

void TFT::displayUpdate() noexcept
{
  // Das Update wird alle 1000mS durchlaufen
  // Bevor Werte ausgegeben werden, wird auf Änderungen der Werte überprüft, nur geänderte Werte werden auf das Display geschrieben

  if (KWLConfig::serialDebugDisplay == 1) {
    Serial.println(F("displayUpdate"));
  }

  // Netzwerkverbindung anzeigen
  tft_.setCursor(20, 0 + BASELINE_MIDDLE);
  tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
  if (!control_->getNetworkClient().isLANOk()) {
    if (last_lan_ok_ != control_->getNetworkClient().isLANOk() || force_display_update_) {
      last_lan_ok_ = control_->getNetworkClient().isLANOk();
      tft_.fillRect(10, 0, 120, 20, colErrorBackColor);
      tft_.setTextColor(colErrorFontColor);
      tft_.print(F("ERR LAN"));
    }
  } else if (!control_->getNetworkClient().isMQTTOk()) {
    if (last_mqtt_ok_ != control_->getNetworkClient().isMQTTOk() || force_display_update_) {
      last_mqtt_ok_ = control_->getNetworkClient().isMQTTOk();
      tft_.fillRect(10, 0, 120, 20, colErrorBackColor);
      tft_.setTextColor(colErrorFontColor );
      tft_.print(F("ERR MQTT"));
    }
  } else {
    last_mqtt_ok_ = true;
    last_lan_ok_ = true;
    tft_.fillRect(10, 0, 120, 20, colBackColor);
    if (control_->getNTP().hasTime()) {
      auto time = control_->getNTP().currentTimeHMS(
            control_->getPersistentConfig().getTimezoneMin() * 60,
            control_->getPersistentConfig().getDST());
      char buffer[6];
      time.writeHM(buffer);
      buffer[5] = 0;
      tft_.setTextColor(colFontColor);
      tft_.print(buffer);
    }
  }

  // Update seite, falls notwendig
  if (display_update_)
    (this->*display_update_)();

  // Zeige Status
  unsigned errors = control_->getErrors();
  unsigned infos = control_->getInfos();
  if (errors != 0) {
    if (errors != last_error_bits_) {
      // Neuer Fehler
      tft_.fillRect(0, 300, 480, 21, colErrorBackColor);
      tft_.setTextColor(colErrorFontColor );
      tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
      tft_.setCursor(18, 301 + BASELINE_SMALL);
      char buffer[80];
      control_->errorsToString(buffer, sizeof(buffer));
      tft_.print(buffer);
      last_error_bits_ = errors;
      tft_.setFont(&FreeSans12pt7b);  // Mittlerer Font
    }
  } else if (infos != 0) {
    if (infos != last_info_bits_) {
      // Neuer Fehler
      tft_.fillRect(0, 300, 480, 21, colInfoBackColor);
      tft_.setTextColor(colInfoFontColor );
      tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
      tft_.setCursor(18, 301 + BASELINE_SMALL);
      char buffer[80];
      control_->infosToString(buffer, sizeof(buffer));
      tft_.print(buffer);
      last_info_bits_ = infos;
      tft_.setFont(&FreeSans12pt7b);  // Mittlerer Font
    }
  }
  else if (last_error_bits_ != 0 || last_info_bits_ != 0) {
    tft_.fillRect(0, 300, 480, 20, colBackColor);
    last_error_bits_ = 0;
    last_info_bits_ = 0;
  }
  force_display_update_ = false;
  display_update_task_.runRepeated(INTERVAL_DISPLAY_UPDATE);
}

// ********************************** Menüfunktionen ******************************************
// menuScreen = Angezeigte Bildschirmseite
// menu_btn_action_[0..5] gesetzt, wenn Menüeintrag sichtbar und aktiviert ist, ansonsten nicht gesetzt

void TFT::gotoScreen(void (TFT::*screenSetup)()) noexcept {
  if (KWLConfig::serialDebugDisplay == 1) {
    Serial.println(F("SetupBackgroundScreen"));
  }

  // Screen Hintergrund
  tft_.fillRect(0, 30, 480 - TOUCH_BTN_WIDTH, 270, colBackColor);

  // Menu Hintergrund
  tft_.fillRect(480 - TOUCH_BTN_WIDTH , TOUCH_BTN_YOFFSET, TOUCH_BTN_WIDTH , 320 - TOUCH_BTN_YOFFSET - 20, colMenuBackColor );

  memset(menu_btn_action_, 0, sizeof(menu_btn_action_));
  display_update_ = nullptr;
  force_display_update_ = true;
  (this->*screenSetup)();
  displayUpdate();
}

template<typename Func>
void TFT::newMenuEntry(byte mnuBtn, const __FlashStringHelper* mnuTxt, Func&& action) noexcept {
  if (mnuBtn < 1 || mnuBtn > MENU_BTN_COUNT) {
    Serial.println(F("Trying to set menu action for invalid index"));
    return;
  }
  menu_btn_action_[mnuBtn - 1] = action;
  printMenuBtn(mnuBtn, mnuTxt, colMenuBtnFrame);
}

void TFT::printMenuBtn(byte mnuBtn, const __FlashStringHelper* mnuTxt, long colFrame) noexcept {
  // ohne mnuText wird vom Button nur der Rand gezeichnet

  if (mnuBtn > 0 && mnuBtn <= MENU_BTN_COUNT && menu_btn_action_[mnuBtn - 1]) {
    int x, y;
    x = 480 - TOUCH_BTN_WIDTH + 1;
    y = TOUCH_BTN_YOFFSET + 1 + TOUCH_BTN_HEIGHT * (mnuBtn - 1);

    if (colFrame == colMenuBtnFrameHL) {
      // Highlight Frame
      tft_.drawRoundRect(x, y, TOUCH_BTN_WIDTH - 2, TOUCH_BTN_HEIGHT - 2, 5, colFrame);
      tft_.drawRoundRect(x + 1, y + 1, TOUCH_BTN_WIDTH - 4, TOUCH_BTN_HEIGHT - 4, 5, colFrame);
      tft_.drawRoundRect(x + 2, y + 2, TOUCH_BTN_WIDTH - 6, TOUCH_BTN_HEIGHT - 6, 5, colFrame);
    } else {
      tft_.drawRoundRect(x, y, TOUCH_BTN_WIDTH - 2, TOUCH_BTN_HEIGHT - 2, 5, colFrame);
      tft_.drawRoundRect(x + 1, y + 1, TOUCH_BTN_WIDTH - 4, TOUCH_BTN_HEIGHT - 4, 5, colMenuBackColor);
      tft_.drawRoundRect(x + 2, y + 2, TOUCH_BTN_WIDTH - 6, TOUCH_BTN_HEIGHT - 6, 5, colMenuBackColor);
    }

    char buffer[8];
    int16_t  x1, y1;
    uint16_t w, h;

    strncpy_P(buffer, reinterpret_cast<const char*>(mnuTxt), 8);
    tft_.setFont(&FreeSans12pt7b);
    tft_.setTextColor(colMenuFontColor, colMenuBackColor);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(480 - TOUCH_BTN_WIDTH / 2 - w / 2 , y + 15 + BASELINE_SMALL);
    tft_.print(buffer);
  }
}

void TFT::setMenuBorder(byte menuBtn) noexcept {
  if (millis() - millis_last_menu_btn_press_ > INTERVAL_MENU_BTN - 100 || menuBtn > 0) {
    if (menuBtn > 0) {
      printMenuBtn(menuBtn, F(""), colMenuBtnFrameHL);
      last_highlighed_menu_btn_ = menuBtn;
    } else {
      printMenuBtn(last_highlighed_menu_btn_, F(""), colMenuBtnFrame);
    }
  }
}

void TFT::printScreenTitle(char* title) noexcept
{
  int16_t  x1, y1;
  uint16_t w, h;

  tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
  tft_.setTextSize(1);
  tft_.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  tft_.setCursor((480 - TOUCH_BTN_WIDTH) / 2 - w / 2, 30 + BASELINE_SMALL);
  tft_.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor);
  tft_.fillRect(0, 30, 480 - TOUCH_BTN_WIDTH, 20, colWindowTitleBackColor);
  tft_.print(title);
  tft_.setTextColor(colFontColor, colBackColor);
}

void TFT::loopTouch() noexcept
{
  if (millis() - millis_last_touch_ > INTERVAL_TOUCH_TIMEOUT) {
    // go to main screen if too long not touched
    millis_last_touch_ = millis();
    if (display_update_ != &TFT::displayUpdateMain)
      gotoScreen(&TFT::screenMain);
  }

  setMenuBorder(0);

  if (millis() - millis_last_menu_btn_press_ > INTERVAL_MENU_BTN) {

    int16_t xpos, ypos;  //screen coordinates
    TSPoint tp = ts_.getPoint();   //tp.x, tp.y are ADC values

    // if sharing pins, you'll need to fix the directions of the touchscreen pins
    pinMode(KWLConfig::XM, OUTPUT);
    pinMode(KWLConfig::YP, OUTPUT);
    pinMode(KWLConfig::XP, OUTPUT);
    pinMode(KWLConfig::YM, OUTPUT);
    //    digitalWrite(XM, HIGH);
    //    digitalWrite(YP, HIGH);
    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!

    if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
      // is controller wired for Landscape ? or are we oriented in Landscape?
      if (KWLConfig::TouchSwapXY != (KWLConfig::TFTOrientation & 1))
        SWAP(tp.x, tp.y);
      // scale from 0->1023 to tft_.width  i.e. left = 0, rt = width
      // most mcufriend have touch (with icons) that extends below the TFT
      // screens without icons need to reserve a space for "erase"
      // scale the ADC values from ts.getPoint() to screen values e.g. 0-239
      xpos = map(tp.x, TS_LEFT, TS_RT, 0, tft_.width());
      ypos = map(tp.y, TS_TOP, TS_BOT, 0, tft_.height());

      millis_last_touch_ = millis();

      if (KWLConfig::serialDebugDisplay) {
        Serial.print(F("Touch (xpos/ypos, tp.x/tp.y): "));
        Serial.print(xpos);
        Serial.print('/');
        Serial.print(ypos);
        Serial.print(',');
        Serial.print(tp.x);
        Serial.print('/');
        Serial.println(tp.y);
      }

      // are we in top color box area ?
      if (xpos > 480 - TOUCH_BTN_WIDTH) {               // Touch im Menübereich, rechte Seite
        auto button = (ypos - TOUCH_BTN_YOFFSET) / TOUCH_BTN_HEIGHT;
        if (button < 6) {
          // touched a button
          byte menuBtnPressed = byte(button + 1);
          setMenuBorder(menuBtnPressed);
          millis_last_menu_btn_press_ = millis();

          if (KWLConfig::serialDebugDisplay) {
            Serial.print(F("DoMenuAction, button="));
            Serial.println(menuBtnPressed);
          }

          // run appropriate menu action
          menu_btn_action_[button].invoke();
        }
      }
    }
  }
}

// **************************** ENDE  Menüfunktionen ******************************************


// *** oberer Rand
void TFT::printHeader() noexcept {
  static constexpr auto VERSION = KWLConfig::VersionString;
  tft_.fillRect(0, 0, 480, 20, colBackColor);
  tft_.setCursor(140, 0 + BASELINE_SMALL);
  tft_.setTextColor(colFontColor);
  tft_.setTextSize(1);
  tft_.print(F(" * Pluggit AP 300 * "));
  tft_.setCursor(420, 0 + BASELINE_SMALL);
  tft_.print(VERSION.load());
}

void TFT::setupDisplay() noexcept
{
  if (KWLConfig::serialDebugDisplay == 1) {
    Serial.println(F("start_tft"));
  }
  auto ID = tft_.readID();  // you must detect the correct controller
  tft_.begin(ID);      // everything will start working

  int16_t  x1, y1;
  uint16_t w, h;
  tft_.setFont(&FreeSansBold24pt7b);  // Großer Font
  // Baseline bestimmen für kleinen Font
  char CharM[] = "M";
  tft_.getTextBounds(CharM, 0, 0, &x1, &y1, &w, &h);
  BASELINE_BIGNUMBER = h;
  Serial.print (F("Font baseline (big / middle / small): "));
  Serial.print (h);
  Serial.print (F(" / "));
  tft_.setFont(&FreeSans12pt7b);  // Mittlerer Font
  // Baseline bestimmen für kleinen Font
  char Char9[] = "9";
  tft_.getTextBounds(Char9, 0, 0, &x1, &y1, &w, &h);
  BASELINE_MIDDLE = h;
  HEIGHT_NUMBER_FIELD = h + 1 ;
  Serial.print (HEIGHT_NUMBER_FIELD);
  Serial.print (F(" / "));
  tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
  // Baseline bestimmen für kleinen Font
  tft_.getTextBounds(CharM, 0, 0, &x1, &y1, &w, &h);
  BASELINE_SMALL = h;
  Serial.print (h);
  Serial.println ();

  Serial.print(F("TFT controller: "));
  Serial.println(ID);
  tft_.setRotation(1);
  tft_.fillScreen(colBackColor);

  printHeader();
  tft_.setCursor(0, 30 + BASELINE_SMALL);
}

void TFT::setupTouch()
{
  uint16_t tmp;
  auto identifier = tft_.readID();
  switch (KWLConfig::TFTOrientation) {      // adjust for different aspects
    case 0:   break;        //no change,  calibrated for PORTRAIT
    case 1:   tmp = TS_LEFT; TS_LEFT = TS_BOT; TS_BOT = TS_RT; TS_RT = TS_TOP; TS_TOP = tmp;  break;
    case 2:   SWAP(TS_LEFT, TS_RT);  SWAP(TS_TOP, TS_BOT); break;
    case 3:   tmp = TS_LEFT; TS_LEFT = TS_TOP; TS_TOP = TS_RT; TS_RT = TS_BOT; TS_BOT = tmp;  break;
  }
  ts_ = TouchScreen(KWLConfig::XP, KWLConfig::YP, KWLConfig::XM, KWLConfig::YM, 300);     //call the constructor AGAIN with new values.

  // Dump debug info:

  Serial.print(F("Found "));
  Serial.println(F(" LCD driver"));
  Serial.print(F("ID = 0x"));
  Serial.println(identifier, HEX);
  Serial.print(F("Screen is "));
  Serial.print(tft_.width());
  Serial.print('x');
  Serial.println(tft_.height());
  Serial.print(F("Calibration is: LEFT = "));
  Serial.print(TS_LEFT);
  Serial.print(F(" RT = "));
  Serial.print(TS_RT);
  Serial.print(F(" TOP = "));
  Serial.print(TS_TOP);
  Serial.print(F(" BOT = "));
  Serial.println(TS_BOT);
  Serial.print(F("Wiring is: "));
  Serial.println(KWLConfig::TouchSwapXY ? F("SwapXY") : F("PORTRAIT"));
  Serial.print(F("YP = "));
  Serial.print(KWLConfig::YP);
  Serial.print(F(" XM = "));
  Serial.println(KWLConfig::XM);
  Serial.print(F("YM = "));
  Serial.print(KWLConfig::YM);
  Serial.print(F(" XP = "));
  Serial.println(KWLConfig::XP);
}
