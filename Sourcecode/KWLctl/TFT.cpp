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
#include "SummerBypass.h"

#include <Adafruit_GFX.h>       // TFT
#include <IPAddress.h>
#include <avr/wdt.h>
#include <alloca.h>

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
/// If the user doesn't confirm the popup within this time, it will be closed automatically.
static constexpr unsigned long POPUP_TIMEOUT_MS = 10000;

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

/*
Screen layout:
   - vertical:
      - 0+30 pixels header, static
      - 30+20 pixels page header, dynamic (also used by menu)
      - 50+270 pixels page contents, dynamic
      - 300+20 pixels status string
   - horizontal:
      - 0+420 drawing area, dynamic
      - 420+60 menu
*/

/// Menu button width.
static constexpr byte TOUCH_BTN_WIDTH = 60;
/// Menu button height.
static constexpr byte TOUCH_BTN_HEIGHT = 45;
/// First menu button Y offset.
static constexpr byte TOUCH_BTN_YOFFSET = 30;

/// Input field height.
static constexpr byte INPUT_FIELD_HEIGHT = 35;
/// First input field Y offset.
static constexpr byte INPUT_FIELD_YOFFSET = 52;

/// Popup width.
static constexpr int POPUP_W = 340;
/// Popup title height.
static constexpr int POPUP_TITLE_H = 30;
/// Popup text area height.
static constexpr int POPUP_H = 150;
/// Popup button width.
static constexpr int POPUP_BTN_W = 60;
/// Popup button height.
static constexpr int POPUP_BTN_H = 30;
/// Popup X position.
static constexpr int POPUP_X = (480 - TOUCH_BTN_WIDTH - POPUP_W) / 2;
/// Popup Y position.
static constexpr int POPUP_Y = (320 - POPUP_H - POPUP_TITLE_H) / 2;
/// Popup button X position.
static constexpr int POPUP_BTN_Y = POPUP_Y + POPUP_TITLE_H + POPUP_H - 10 - POPUP_BTN_H;
/// Popup button Y position.
static constexpr int POPUP_BTN_X = (480 - TOUCH_BTN_WIDTH - POPUP_BTN_W) / 2;

static const __FlashStringHelper* bypassModeToString(SummerBypassFlapState mode) noexcept
{
  switch (mode) {
    default:
    case SummerBypassFlapState::UNKNOWN:
      return F("automatisch");
    case SummerBypassFlapState::CLOSED:
      return F("manuell geschl.");
    case SummerBypassFlapState::OPEN:
      return F("manuell offen");
  }
}

static const char* fanModeToString(FanCalculateSpeedMode mode) noexcept
{
  switch (mode) {
    default:
    case FanCalculateSpeedMode::PID:
      return PSTR("PID-Regler");
    case FanCalculateSpeedMode::PROP:
      return PSTR("PWM-Wert");
  }
}

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

// ****************************************** Screen: HAUPTSEITE ******************************************
void TFT::screenMain() noexcept
{
  printScreenTitle(F("Lueftungsstufe"));

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
  int speed1 = int(control_->getFanControl().getFan1().getSpeed());
  if (abs(screen_state_.main_.tacho_fan1_ - speed1) > 10) {
    screen_state_.main_.tacho_fan1_ = speed1;
    tft_.fillRect(280, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    snprintf(buffer, sizeof(buffer), "%5i", speed1);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(340 - int(w), 192 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // Speed Fan2
  int speed2 = int(control_->getFanControl().getFan2().getSpeed());
  if (abs(screen_state_.main_.tacho_fan2_ - speed2) > 10) {
    screen_state_.main_.tacho_fan2_ = speed2;
    snprintf(buffer, sizeof(buffer), "%5i", speed2);
    // Debug einkommentieren
    // tft_.fillRect(280, 218, 60, HEIGHT_NUMBER_FIELD, colWindowTitle);
    tft_.fillRect(280, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(340 - int(w), 218 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T1
  if (abs(screen_state_.main_.t1_ - control_->getTempSensors().get_t1_outside()) > 0.5) {
    screen_state_.main_.t1_ = control_->getTempSensors().get_t1_outside();
    tft_.fillRect(160, 166, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t1_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - int(w), 166 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T2
  if (abs(screen_state_.main_.t2_ - control_->getTempSensors().get_t2_inlet()) > 0.5) {
    screen_state_.main_.t2_ = control_->getTempSensors().get_t2_inlet();
    tft_.fillRect(160, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t2_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - int(w), 192 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T3
  if (abs(screen_state_.main_.t3_ - control_->getTempSensors().get_t3_outlet()) > 0.5) {
    screen_state_.main_.t3_ = control_->getTempSensors().get_t3_outlet();
    tft_.fillRect(160, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t3_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - int(w), 218 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // T4
  if (abs(screen_state_.main_.t4_ - control_->getTempSensors().get_t4_exhaust()) > 0.5) {
    screen_state_.main_.t4_ = control_->getTempSensors().get_t4_exhaust();
    tft_.fillRect(160, 244, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    dtostrf(screen_state_.main_.t4_, 5, 1, buffer);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - int(w), 244 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
  // Etha Wirkungsgrad
  if (abs(screen_state_.main_.efficiency_ - control_->getTempSensors().getEfficiency()) > 1) {
    screen_state_.main_.efficiency_ = control_->getTempSensors().getEfficiency();
    tft_.fillRect(160, 270, 80, HEIGHT_NUMBER_FIELD, colBackColor);
    snprintf(buffer, sizeof(buffer), "%5d %%", screen_state_.main_.efficiency_);
    tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor(240 - int(w), 270 + BASELINE_MIDDLE);
    tft_.print(buffer);
  }
}

// ************************************ ENDE: Screen HAUPTSEITE *****************************************************

// ****************************************** Screen: WEITERE SENSORWERTE *********************************
void TFT::screenAdditionalSensors() noexcept
{
  printScreenTitle(F("Weitere Sensorwerte"));

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

void TFT::displayUpdateAdditionalSensors() noexcept
{
  tft_.setFont(&FreeSans12pt7b);
  tft_.setTextColor(colFontColor, colBackColor);

  // TODO what about % humidity?
  char buffer[16];
  int16_t  x1, y1;
  uint16_t w, h;

  // DHT 1
  if (control_->getAdditionalSensors().hasDHT1()) {
    if (abs(double(screen_state_.addt_sensors_.dht1_temp_ - control_->getAdditionalSensors().getDHT1Temp())) > 0.1) {
      screen_state_.addt_sensors_.dht1_temp_ = control_->getAdditionalSensors().getDHT1Temp();
      tft_.fillRect(160, 166, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(double(screen_state_.addt_sensors_.dht1_temp_), 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - int(w), 166 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
  }
  // DHT 2
  if (control_->getAdditionalSensors().hasDHT2()) {
    if (abs(double(screen_state_.addt_sensors_.dht2_temp_ - control_->getAdditionalSensors().getDHT2Temp())) > 0.1) {
      screen_state_.addt_sensors_.dht2_temp_ = control_->getAdditionalSensors().getDHT2Temp();
      tft_.fillRect(160, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(double(screen_state_.addt_sensors_.dht2_temp_), 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - int(w), 192 + BASELINE_MIDDLE);
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
      tft_.setCursor(240 - int(w), 218 + BASELINE_MIDDLE);
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
      tft_.setCursor(240 - int(w), 244 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
  }
}

// ************************************ ENDE: Screen WEITERE SENSORWERTE  *****************************************************

// ****************************************** Screen: SETUP ******************************
void TFT::screenSetup() noexcept
{
  printScreenTitle(F("Einstellungen"));

  tft_.setTextColor(colFontColor, colBackColor );

  tft_.setCursor(18, 121 + BASELINE_MIDDLE);
  tft_.print (F("FAN: Enstellungen Ventilatoren"));
  tft_.setCursor(18, 166 + BASELINE_MIDDLE);
  tft_.print (F("BYP: Einstellungen Sommer-Bypass"));
  tft_.setCursor(18, 211 + BASELINE_MIDDLE);
  tft_.print(F("NET: Netzwerkeinstellungen"));
//  tft_.setCursor(18, 256 + BASELINE_MIDDLE);
//  tft_.print(F("XXX: xxx"));

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
  newMenuEntry(3, F("FAN"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupFan);
    }
  );
  newMenuEntry(4, F("BYP"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupBypass);
    }
  );
  newMenuEntry(5, F("NET"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetupIPAddress);
    }
  );
//  newMenuEntry(6, F("XXX"),
//    [this]() noexcept {
//      gotoScreen(&TFT::screenSetupXXX);
//    }
//  );
}

// ************************************ ENDE: Screen SETUP  *****************************************************

// ****************************************** Screen: FAN EINSTELLUNGEN ******************************
void TFT::screenSetupFan() noexcept
{
  printScreenTitle(F("Einstellungen Ventilatoren"));

  // copy current state
  auto& ref = screen_state_.fan_;
  auto& config = control_->getPersistentConfig();
  ref.setpoint_l1_ = config.getSpeedSetpointFan1();
  ref.setpoint_l2_ = config.getSpeedSetpointFan2();
  ref.calculate_speed_mode_ = control_->getFanControl().getCalculateSpeedMode();

  setupInputAction(
    [this]() {
      // input enter action
      char buf[16];
      auto& ref = screen_state_.fan_;
      switch (input_current_row_) {
        default:
        case 1: snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.setpoint_l1_); break;
        case 2: snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.setpoint_l2_); break;
        case 3: strcpy_P(buf, fanModeToString(ref.calculate_speed_mode_)); break;
      }
      drawCurrentInputField(buf, true);
    },
    [this]() {
      // input leave action
      input_field_draw_.invoke();
    },
    [this]() {
      // draw action
      char buf[16];
      auto& ref = screen_state_.fan_;
      switch (input_current_row_) {
        default:
        case 1: snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.setpoint_l1_); break;
        case 2: snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.setpoint_l2_); break;
        case 3: strcpy_P(buf, fanModeToString(ref.calculate_speed_mode_)); break;
      }
      drawCurrentInputField(buf, false);
    }
  );

  setupInputFieldColumns(280, 70);
  setupInputFieldRow(1, 1, F("Normdrehzahl Zuluft:"));
  setupInputFieldRow(2, 1, F("Normdrehzahl Abluft:"));
  setupInputFieldRow(3, 1, F("Luefterregelung:"));

  tft_.setFont(&FreeSans9pt7b);
  tft_.setTextColor(colFontColor, colBackColor);
  tft_.setCursor(18, 125 + BASELINE_MIDDLE);
  tft_.print (F("Nach der Aenderung der Normdrehzahlen"));
  tft_.setCursor(18, 150 + BASELINE_MIDDLE);
  tft_.print (F("der Luefter muessen diese kalibriert werden."));
  tft_.setCursor(18, 175 + BASELINE_MIDDLE);
  tft_.print (F("Bei der Kalibrierung werden die Drehzahlen"));
  tft_.setCursor(18, 200 + BASELINE_MIDDLE);
  tft_.print (F("der Luefter eingestellt und die notwendigen"));
  tft_.setCursor(18, 225 + BASELINE_MIDDLE);
  tft_.print (F("PWM-Werte für jede Stufe gespeichert."));

  newMenuEntry(1, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(2, F("+"),
    [this]() noexcept {
      auto& ref = screen_state_.fan_;
      switch (input_current_row_) {
        case 1:
          ref.setpoint_l1_ += 10;
          if (ref.setpoint_l1_ > FanRPM::MAX_RPM)
            ref.setpoint_l1_ = FanRPM::MAX_RPM;
          break;
        case 2:
          ref.setpoint_l2_ += 10;
          if (ref.setpoint_l2_ > FanRPM::MAX_RPM)
            ref.setpoint_l2_ = FanRPM::MAX_RPM;
          break;
        case 3:
          ref.calculate_speed_mode_ = FanCalculateSpeedMode::PID;
          break;
      }
      input_field_enter_.invoke();
    }
  );
  newMenuEntry(3, F("-"),
    [this]() noexcept {
      auto& ref = screen_state_.fan_;
      switch (input_current_row_) {
        case 1:
          if (ref.setpoint_l1_ > FanRPM::MIN_RPM + 10)
            ref.setpoint_l1_ -= 10;
          else
            ref.setpoint_l1_ = FanRPM::MIN_RPM;
          break;
        case 2:
          if (ref.setpoint_l2_ > FanRPM::MIN_RPM + 10)
            ref.setpoint_l2_ -= 10;
          else
            ref.setpoint_l2_ = FanRPM::MIN_RPM;
          break;
        case 3:
          ref.calculate_speed_mode_ = FanCalculateSpeedMode::PROP;
          break;
       }
       input_field_enter_.invoke();
     }
  );
  newMenuEntry(4, F("OK"),
    [this]() noexcept {
      resetInput();
      // write to EEPROM and restart
      auto& ref = screen_state_.fan_;
      auto& config = control_->getPersistentConfig();
      bool changed =
          config.getSpeedSetpointFan1() != ref.setpoint_l1_ ||
          config.getSpeedSetpointFan2() != ref.setpoint_l2_;
      auto msg = F("Neuer Modus wurde gespeichert.");
      auto screen = &TFT::screenSetup;
      if (changed) {
        config.setSpeedSetpointFan1(ref.setpoint_l1_);
        config.setSpeedSetpointFan2(ref.setpoint_l2_);
        msg = F("Nenndrehzahlen geaendert.\nBitte Kalibrierung starten.");
        screen = &TFT::screenSetupFan;
      }
      control_->getFanControl().setCalculateSpeedMode(ref.calculate_speed_mode_);
      doPopup(
        F("Einstellungen gespeichert"),
        msg, screen);
    }
  );
  newMenuEntry(6, F("KAL"),
    [this]() noexcept {
      auto& ref = screen_state_.fan_;
      auto& config = control_->getPersistentConfig();
      config.setSpeedSetpointFan1(ref.setpoint_l1_);
      config.setSpeedSetpointFan2(ref.setpoint_l2_);
      control_->getFanControl().setCalculateSpeedMode(ref.calculate_speed_mode_);
      control_->getFanControl().speedCalibrationStart();
      doPopup(F("Kalibrierung"), F("Luefterkalibrierung wurde gestartet."), &TFT::screenSetup);
    }
  );
}

// ************************************ ENDE: Screen FAN EINSTELLUNGEN *****************************************************

// ****************************************** Screen: NETZWERKEINSTELLUNGEN ******************************
void TFT::screenSetupIPAddress() noexcept
{
  printScreenTitle(F("Netzwerkeinstellungen"));

  // copy IP addresses and ports
  auto& ref = screen_state_.net_;
  auto& config = control_->getPersistentConfig();
  ref.ip_ = config.getNetworkIPAddress();
  ref.mask_ = config.getNetworkSubnetMask();
  ref.gw_ = config.getNetworkGateway();
  ref.mqtt_ = config.getNetworkMQTTBroker();
  ref.mqtt_port_ = config.getNetworkMQTTPort();
  ref.ntp_ = config.getNetworkNTPServer();
  ref.dns_ = config.getNetworkDNSServer();
  ref.mac_ = config.getNetworkMACAddress();

  tft_.setTextColor(colFontColor, colBackColor );

  setupInputAction(
    [this]() {
      // input enter action
      char buf[8];
      auto& ref = screen_state_.net_;
      IPAddressLiteral* ip;
      switch (input_current_row_) {
        default:
        case 1: ip = &ref.ip_; break;
        case 2: ip = &ref.mask_; break;
        case 3: ip = &ref.gw_; break;
        case 4: ip = &ref.mqtt_; break;
        case 5: ip = nullptr; ref.cur_ = ref.mqtt_port_; break;
        case 6: ip = &ref.ntp_; break;
        case 7: ip = nullptr; ref.cur_ = ref.mac_[2 + input_current_col_]; break;
      }
      if (ip)
        ref.cur_ = (*ip)[input_current_col_];
      if (input_current_row_ != 7)
        snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.cur_);
      else
        snprintf_P(buf, sizeof(buf), PSTR("%02x"), ref.cur_);
      drawCurrentInputField(buf, true);
    },
    [this]() {
      // input leave action
      auto& ref = screen_state_.net_;
      IPAddress old_ip(ref.ip_);
      IPAddress old_mask(ref.mask_);

      IPAddressLiteral* ip;
      switch (input_current_row_) {
        default:
        case 1: ip = &ref.ip_; break;
        case 2: ip = &ref.mask_; break;
        case 3: ip = &ref.gw_; break;
        case 4: ip = &ref.mqtt_; break;
        case 5: ip = nullptr; ref.mqtt_port_ = ref.cur_; break;
        case 6: ip = &ref.ntp_; break;
        case 7: ip = nullptr; ref.mac_[2 + input_current_col_] = uint8_t(ref.cur_); break;
      }
      if (ip)
        (*ip)[input_current_col_] = uint8_t(ref.cur_);
      char buf[8];
      if (input_current_row_ != 7)
        snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.cur_);
      else
        snprintf_P(buf, sizeof(buf), PSTR("%02x"), ref.cur_);
      drawCurrentInputField(buf, false);

      IPAddress new_ip(ref.ip_);
      IPAddress new_mask(ref.mask_);
      IPAddress old_gw(ref.gw_);
      {
        IPAddress gw((new_ip & new_mask) | (old_gw & ~old_mask));
        if (gw != old_gw) {
          screenSetupIPAddressUpdateAddress(3, ref.gw_, gw);
          old_gw = gw;
        }
      }
      if (input_current_row_ <= 3) {
        // gateway potentially changed, check NTP and MQTT
        IPAddress new_gw(ref.gw_);
        if (old_gw == IPAddress(ref.mqtt_))
          screenSetupIPAddressUpdateAddress(4, ref.mqtt_, new_gw);
        if (old_gw == IPAddress(ref.ntp_))
          screenSetupIPAddressUpdateAddress(6, ref.ntp_, new_gw);
      }
      if (input_current_row_ <= 2) {
        // netmask or IP changed, check other values
        {
          IPAddress mqtt(ref.mqtt_);
          if ((old_ip & old_mask) == (mqtt & old_mask)) {
            // mqtt server is on the same network, update it
            mqtt = (new_ip & new_mask) | (mqtt & ~old_mask);
            screenSetupIPAddressUpdateAddress(4, ref.mqtt_, mqtt);
          }
        }
        {
          IPAddress ntp(ref.ntp_);
          if ((old_ip & old_mask) == (ntp & old_mask)) {
            // NTP server is on the same network, update it
            ntp = (new_ip & new_mask) | (ntp & ~old_mask);
            screenSetupIPAddressUpdateAddress(6, ref.ntp_, ntp);
          }
        }
        {
          IPAddress dns(ref.dns_);
          if ((old_ip & old_mask) == (dns & old_mask)) {
            // DNS server is on the same network, update it (not displayed)
            ref.dns_ = IPAddressLiteral((new_ip & new_mask) | (dns & ~old_mask));
          }
        }
      }
    },
    [this]() {
      // draw action
      char buf[8];
      auto& ref = screen_state_.net_;
      IPAddressLiteral* ip;
      uint16_t cur;
      switch (input_current_row_) {
        default:
        case 1: ip = &ref.ip_; break;
        case 2: ip = &ref.mask_; break;
        case 3: ip = &ref.gw_; break;
        case 4: ip = &ref.mqtt_; break;
        case 5: ip = nullptr; cur = ref.mqtt_port_; break;
        case 6: ip = &ref.ntp_; break;
        case 7: ip = nullptr; cur = ref.mac_[2 + input_current_col_]; break;
      }
      if (ip)
        cur = (*ip)[input_current_col_];
      if (input_current_row_ != 7)
        snprintf_P(buf, sizeof(buf), PSTR("%u"), cur);
      else
        snprintf_P(buf, sizeof(buf), PSTR("%02x"), cur);
      drawCurrentInputField(buf, false);
    }
  );

  setupInputFieldColumns(195, 45);
  input_w_[0] = 70;
  input_x_[0] = 170;
  setupInputFieldRow(1, 4, F("IP Adresse:"), F("."));
  setupInputFieldRow(2, 4, F("Netzmaske:"), F("."));
  setupInputFieldRow(3, 4, F("Gateway:"), F("."));
  setupInputFieldRow(4, 4, F("MQTT IP:"), F("."));
  setupInputFieldRow(5, 1, F("MQTT port:"));
  setupInputFieldRow(6, 4, F("NTP IP:"), F("."));
  {
    char buffer[16];
    snprintf_P(buffer, sizeof(buffer), PSTR("MAC:  %02x: %02x:"), ref.mac_[0], ref.mac_[1]);
    setupInputFieldRow(7, 4, buffer, F(":"));
  }

  newMenuEntry(1, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(2, F("+10"),
    [this]() noexcept {
      if (input_current_row_ == 7)
        screenSetupIPAddressUpdate(16);
      else
        screenSetupIPAddressUpdate(10);
    }
  );
  newMenuEntry(3, F("+1"),
    [this]() noexcept {
      screenSetupIPAddressUpdate(1);
    }
  );
  newMenuEntry(4, F("-1"),
    [this]() noexcept {
      screenSetupIPAddressUpdate(-1);
    }
  );
  newMenuEntry(5, F("-10"),
    [this]() noexcept {
      if (input_current_row_ == 7)
        screenSetupIPAddressUpdate(-16);
      else
        screenSetupIPAddressUpdate(-10);
    }
  );
  newMenuEntry(6, F("OK"),
    [this]() noexcept {
      resetInput();
      // write to EEPROM and restart
      auto& ref = screen_state_.net_;
      auto& config = control_->getPersistentConfig();
      config.setNetworkIPAddress(ref.ip_);
      config.setNetworkSubnetMask(ref.mask_);
      config.setNetworkGateway(ref.gw_);
      config.setNetworkMQTTBroker(ref.mqtt_);
      config.setNetworkMQTTPort(ref.mqtt_port_);
      config.setNetworkNTPServer(ref.ntp_);
      config.setNetworkDNSServer(ref.dns_);
      doRestart(
        F("Einstellungen gespeichert"),
        F("Die Steuerung wird jetzt neu gestartet."));
    }
  );
}

void TFT::screenSetupIPAddressUpdate(int delta) noexcept
{
  auto& ref = screen_state_.net_;
  uint16_t new_value;
  if (input_current_row_ == 2) {
    // netmask change is special, it shifts bits
    if (delta < 0) {
      // shift to left
      new_value = (ref.cur_ << 1) & 255;
    } else {
      new_value = (ref.cur_ >> 1) | 128;
    }
  } else {
    if (delta < 0) {
      delta = -delta;
      if (unsigned(delta) > ref.cur_)
        new_value = 0;
      else
        new_value = ref.cur_ - unsigned(delta);
    } else {
      new_value = ref.cur_ + unsigned(delta);
      if (input_current_row_ == 5) {
        // MQTT port can have any value
        if (new_value < ref.cur_)
          new_value = 65535;  // overflow
      } else if (new_value > 255) {
        new_value = 255;
      }
    }
  }
  if (new_value == ref.cur_)
    return; // no change

  if (input_current_row_ == 2) {
    // new netmask validation
    if (new_value != 0) {
      // ensure everything to the left is 255
      auto current_col = input_current_col_;
      for (input_current_col_ = 0; input_current_col_ < current_col; ++input_current_col_) {
        if (ref.mask_[input_current_col_] != 255) {
          ref.mask_[input_current_col_] = 255;
          input_field_draw_.invoke();
        }
      }
    }
    if (new_value != 255) {
      // ensure everything to the right is 0
      auto current_col = input_current_col_;
      for (input_current_col_ = 3; input_current_col_ > current_col; --input_current_col_) {
        if (ref.mask_[input_current_col_] != 0) {
          ref.mask_[input_current_col_] = 0;
          input_field_draw_.invoke();
        }
      }
    }
  }

  ref.cur_ = new_value;
  char buf[8];
  if (input_current_row_ != 7)
    snprintf_P(buf, sizeof(buf), PSTR("%d"), new_value);
  else
    snprintf_P(buf, sizeof(buf), PSTR("%02x"), new_value);
  drawCurrentInputField(buf, true);
}

void TFT::screenSetupIPAddressUpdateAddress(uint8_t row, IPAddressLiteral& ip, IPAddress new_ip) noexcept
{
  // called from leave function to update some addresses
  auto old_row = input_current_row_;
  auto old_col = input_current_col_;
  input_current_row_ = row;
  for (uint8_t i = 0; i < 4; ++i) {
    if (ip[i] != new_ip[i]) {
      input_current_col_ = i;
      ip[i] = new_ip[i];
      input_field_draw_.invoke();
    }
  }
  input_current_row_ = old_row;
  input_current_col_ = old_col;
}

// ************************************ ENDE: Screen NETZWERKEINSTELLUNGEN  *****************************************************

// ****************************************** Screen: BYPASSEINSTELLUNGEN ******************************
void TFT::screenSetupBypass() noexcept
{
  printScreenTitle(F("Einstellungen Bypass"));

  // copy bypass settings
  auto& ref = screen_state_.bypass_;
  auto& config = control_->getPersistentConfig();
  ref.temp_outside_min_ = config.getBypassTempAussenluftMin();
  ref.temp_outtake_min_ = config.getBypassTempAbluftMin();
  ref.temp_hysteresis_ = config.getBypassHysteresisTemp();
  ref.min_hysteresis_ = config.getBypassHystereseMinutes();
  if (config.getBypassMode() == SummerBypassMode::USER)
    ref.mode_ = unsigned(config.getBypassManualSetpoint());  // manual (1=closed, 2=open)
  else
    ref.mode_ = unsigned(SummerBypassFlapState::UNKNOWN); // auto

  tft_.setTextColor(colFontColor, colBackColor );

  setupInputAction(
    [this]() {
      // input enter action
      auto& ref = screen_state_.bypass_;
      switch (input_current_row_) {
        default:
        case 1: ref.cur_ = ref.temp_outtake_min_; break;
        case 2: ref.cur_ = ref.temp_outside_min_; break;
        case 3: ref.cur_ = ref.temp_hysteresis_; break;
        case 4: ref.cur_ = ref.min_hysteresis_; break;
        case 5: ref.cur_ = ref.mode_; break;
      }
      if (input_current_row_ != 5) {
        char buf[8];
        snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.cur_);
        drawCurrentInputField(buf, true, false);
      } else {
        drawCurrentInputField(bypassModeToString(SummerBypassFlapState(ref.cur_)), true, false);
      }
    },
    [this]() {
      // input leave action
      auto& ref = screen_state_.bypass_;
      switch (input_current_row_) {
        default:
        case 1: ref.temp_outtake_min_ = ref.cur_; break;
        case 2: ref.temp_outside_min_ = ref.cur_; break;
        case 3: ref.temp_hysteresis_ = uint8_t(ref.cur_); break;
        case 4: ref.min_hysteresis_ = ref.cur_; break;
        case 5: ref.mode_ = ref.cur_; break;
      }
      if (input_current_row_ != 5) {
        char buf[8];
        snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.cur_);
        drawCurrentInputField(buf, false, false);
      } else {
        drawCurrentInputField(bypassModeToString(SummerBypassFlapState(ref.cur_)), false, false);
      }
    },
    [this]() {
      // draw action
      auto& ref = screen_state_.bypass_;
      unsigned cur;
      switch (input_current_row_) {
        default:
        case 1: cur = ref.temp_outtake_min_; break;
        case 2: cur = ref.temp_outside_min_; break;
        case 3: cur = ref.temp_hysteresis_; break;
        case 4: cur = ref.min_hysteresis_; break;
        case 5: cur = ref.mode_; break;
      }
      if (input_current_row_ != 5) {
        char buf[8];
        snprintf_P(buf, sizeof(buf), PSTR("%u"), cur);
        drawCurrentInputField(buf, false, false);
      } else {
        drawCurrentInputField(bypassModeToString(SummerBypassFlapState(cur)), false, false);
      }
    }
  );

  setupInputFieldColumns(240, 170);
  setupInputFieldRow(1, 1, F("Temp. Abluft Min:"));
  setupInputFieldRow(2, 1, F("Temp. Aussen Min:"));
  setupInputFieldRow(3, 1, F("Temp. Hysteresis:"));
  setupInputFieldRow(4, 1, F("Hysteresis Minuten:"));
  setupInputFieldRow(5, 1, F("Modus:"));

  newMenuEntry(1, F("<-"),
    [this]() noexcept {
      gotoScreen(&TFT::screenSetup);
    }
  );
  newMenuEntry(3, F("+"),
    [this]() noexcept {
      screenSetupBypassUpdate(1);
    }
  );
  newMenuEntry(4, F("-"),
    [this]() noexcept {
      screenSetupBypassUpdate(-1);
    }
  );
  newMenuEntry(6, F("OK"),
    [this]() noexcept {
      resetInput();
      auto& config = control_->getPersistentConfig();
      auto& ref = screen_state_.bypass_;
      config.setBypassTempAussenluftMin(ref.temp_outside_min_);
      config.setBypassTempAbluftMin(ref.temp_outtake_min_);
      config.setBypassHysteresisTemp(ref.temp_hysteresis_);
      config.setBypassHystereseMinutes(ref.min_hysteresis_);
      if (ref.mode_ == unsigned(SummerBypassFlapState::UNKNOWN)) {
        config.setBypassMode(SummerBypassMode::AUTO);
      } else {
        config.setBypassMode(SummerBypassMode::USER);
        config.setBypassManualSetpoint(SummerBypassFlapState(ref.mode_));
      }
      doPopup(
        F("Einstellungen gespeichert"),
        F("Neue Bypasseinstellungen wurden\nin EEPROM gespeichert\nund sind sofort aktiv."),
        &TFT::screenSetup);
    }
  );
}

void TFT::screenSetupBypassUpdate(int delta) noexcept
{
  auto& ref = screen_state_.bypass_;
  int min, max;
  switch (input_current_row_) {
    case 1: min = 5; max = 30; break;
    case 2: min = 3; max = 25; break;
    case 3: min = 1; max = 5; break;
    case 4: min = 5; max = 90; break;
    case 5: min = 0; max = 2; break;
  }
  delta += ref.cur_;
  if (delta < min)
    delta = min;
  if (delta > max)
    delta = max;
  if (unsigned(delta) == ref.cur_)
    return;
  ref.cur_ = unsigned(delta);
  if (input_current_row_ != 5) {
    char buf[8];
    snprintf_P(buf, sizeof(buf), PSTR("%u"), ref.cur_);
    drawCurrentInputField(buf, true, false);
  } else {
    drawCurrentInputField(bypassModeToString(SummerBypassFlapState(ref.cur_)), true, false);
  }
}

// ************************************ ENDE: Screen BYPASSEINSTELLUNGEN *****************************************************

// ****************************************** Screen: WERKSEINSTELLUNGEN **********************************
void TFT::screenSetupFactoryDefaults() noexcept
{
  printScreenTitle(F("Ruecksetzen auf Werkseinstellungen"));

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
      Serial.print(F("Speicherbereich wird geloescht... "));
      tft_.setFont(&FreeSans9pt7b);
      tft_.setTextColor(colFontColor, colBackColor);
      tft_.setCursor(18, 220 + BASELINE_MIDDLE);
      tft_.print(F("Speicherbereich wird geloescht... "));

      control_->getPersistentConfig().factoryReset();
      tft_.println(F("OK"));
      Serial.println(F("OK"));

      doRestart(
        F("Einstellungen gespeichert"),
        F("Werkseinstellungen wiederhergestellt\nDie Steuerung wird jetzt neu gestartet."));
    }
  );
}

// ************************************ ENDE: Screen WERKSEINSTELLUNGEN  *****************************************************

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

  // Update Seite, falls notwendig
  if (display_update_ && !popup_action_)
    (this->*display_update_)();

  // Zeige Status
  unsigned errors = control_->getErrors() & ~(KWLControl::ERROR_BIT_CRASH | KWLControl::ERROR_BIT_NTP);
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
      // Neue Infomessage
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
    // Keine Fehler oder Infos
    tft_.fillRect(0, 300, 480, 21, colBackColor);
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
  for (uint8_t i = 0; i < INPUT_ROW_COUNT; ++i)
    input_active_[i] = 0;
  input_current_col_ = input_current_row_ = 0;
  input_field_enter_ = nullptr;
  input_field_leave_ = nullptr;
  input_field_draw_ = nullptr;
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

void TFT::printMenuBtn(byte mnuBtn, const __FlashStringHelper* mnuTxt, uint16_t colFrame) noexcept {
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

void TFT::printScreenTitle(const __FlashStringHelper* title) noexcept
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

template<typename Func1, typename Func2, typename Func3>
void TFT::setupInputAction(Func1&& start_input, Func2&& stop_input, Func3&& draw) noexcept
{
  input_field_enter_ = start_input;
  input_field_leave_ = stop_input;
  input_field_draw_ = draw;
  setupInputFieldColumns();
}

void TFT::setupInputFieldColumns(int left, int width, int spacing) noexcept
{
  for (uint8_t i = 0; i < INPUT_COL_COUNT; ++i)
  {
    input_x_[i] = left;
    input_w_[i] = width;
    left += (width + spacing);
  }
}

void TFT::setupInputFieldRow(uint8_t row, uint8_t count, const __FlashStringHelper* header, const __FlashStringHelper* separator) noexcept
{
  if (row == 0 || row > INPUT_ROW_COUNT)
    return; // error
  --row;
  int y = INPUT_FIELD_YOFFSET + 1 + INPUT_FIELD_HEIGHT * row;
  input_active_[row] = uint8_t(1 << count) - 1;

  if (separator) {
    // print separator(s)
    int16_t  x1, y1;
    uint16_t tw, th;
    tft_.getTextBounds(separator, 0, 0, &x1, &y1, &tw, &th);
    for (uint8_t i = 0; i < count - 1; ++i) {
      int x1 = input_x_[i] + input_w_[i];
      int x2 = input_x_[i + 1];
      tft_.setCursor((x1 + x2 - int(tw)) / 2, y + 15 + BASELINE_SMALL);
      tft_.print(separator);
    }
  }

  // draw input fields
  input_current_row_ = row + 1;
  for (uint8_t i = 0; i < count; ++i) {
    input_current_col_ = i;
    input_field_draw_.invoke();
  }
  input_current_row_ = input_current_col_ = 0;

  // print row header
  tft_.setTextColor(colFontColor, colBackColor);
  tft_.setFont(&FreeSans12pt7b);
  tft_.setCursor(18, y + 15 + BASELINE_SMALL);
  tft_.print(header);
}

void TFT::setupInputFieldRow(uint8_t row, uint8_t count, const char* header, const __FlashStringHelper* separator) noexcept
{
  setupInputFieldRow(row, count, F(""), separator);
  tft_.print(header);
}

void TFT::resetInput() noexcept
{
  if (input_current_row_) {
    input_field_leave_.invoke();
    input_current_col_ = input_current_row_ = 0;
  }
}


void TFT::drawInputField(uint8_t row, uint8_t col, const char* text, bool highlight, bool right_align) noexcept
{
  if (row == 0 || row > INPUT_ROW_COUNT)
    return; // error
  --row;

  int x = input_x_[col];
  int w = input_w_[col];
  int y = INPUT_FIELD_YOFFSET + 1 + INPUT_FIELD_HEIGHT * row;

  if (highlight) {
    // Highlighted field
    tft_.fillRect(x, y, w, INPUT_FIELD_HEIGHT, colMenuBackColor);
    tft_.setTextColor(colMenuFontColor, colMenuBackColor);
  } else {
    // Normal field
    tft_.fillRect(x, y, w, INPUT_FIELD_HEIGHT, colBackColor);
    tft_.setTextColor(colFontColor, colBackColor);
  }

  int16_t  x1, y1;
  uint16_t tw, th;

  tft_.setFont(&FreeSans12pt7b);
  tft_.getTextBounds(const_cast<char*>(text), 0, 0, &x1, &y1, &tw, &th);
  if (right_align)
    x += w - 10 - int(tw);
  tft_.setCursor(x + 5, y + 15 + BASELINE_SMALL);
  tft_.print(text);
}

void TFT::drawInputField(uint8_t row, uint8_t col, const __FlashStringHelper* text, bool highlight, bool right_align) noexcept
{
  char* buffer = reinterpret_cast<char*>(alloca(strlen_P(reinterpret_cast<const char*>(text)) + 1));
  strcpy_P(buffer, reinterpret_cast<const char*>(text));
  drawInputField(row, col, buffer, highlight, right_align);
}

void TFT::loopTouch() noexcept
{
  // First check timeouts
  if (popup_action_) {
    // there is a popup active
    if (millis() - millis_popup_show_time_ > POPUP_TIMEOUT_MS) {
      // popup timed out, do the default action
      auto tmp = popup_action_;
      popup_action_ = nullptr;
      gotoScreen(tmp);
      return;
    }
  } else if (millis() - millis_last_touch_ > INTERVAL_TOUCH_TIMEOUT) {
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
      xpos = int(map(tp.x, TS_LEFT, TS_RT, 0, tft_.width()));
      ypos = int(map(tp.y, TS_TOP, TS_BOT, 0, tft_.height()));

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

      if (popup_action_) {
        // popup is showing, check for OK button only
        if (xpos >= POPUP_BTN_X - 10 && xpos < POPUP_BTN_X + POPUP_BTN_W + 10 &&
            ypos >= POPUP_BTN_Y - 10 && ypos < POPUP_BTN_Y + POPUP_BTN_H + 10) {
          // popup button hit, call action
          auto tmp = popup_action_;
          popup_action_ = nullptr;
          gotoScreen(tmp);
        }
      } else if (xpos >= 480 - TOUCH_BTN_WIDTH) {
        // we are in menu area
        auto button = (ypos - TOUCH_BTN_YOFFSET) / TOUCH_BTN_HEIGHT;
        if (unsigned(button) < MENU_BTN_COUNT) {
          // touched a menu button
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
      } else {
        // we are in main display area
        auto button = (ypos - INPUT_FIELD_YOFFSET) / INPUT_FIELD_HEIGHT;
        if (unsigned(button) < INPUT_ROW_COUNT && input_active_[button]) {
          // potentially touched an input field
          auto mask = input_active_[button];
          for (uint8_t i = 0, bit = 1; i < INPUT_COL_COUNT; ++i, bit <<= 1) {
            if ((mask & bit) == 0)
              continue;
            // active input field
            if (xpos >= input_x_[i] && xpos < input_x_[i] + input_w_[i]) {
              // hit active input
              if (input_current_row_ != button + 1 || input_current_col_ != i) {
                if (input_current_row_)
                  input_field_leave_.invoke();
                input_current_col_ = i;
                input_current_row_ = uint8_t(button + 1);
                input_field_enter_.invoke();
              }
              break;
            }
          }
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

void TFT::doRestart(const __FlashStringHelper* title, const __FlashStringHelper* message) noexcept
{
  doPopup(title, message, &TFT::doRestartImpl);
}

void TFT::doPopup(const __FlashStringHelper* title, const __FlashStringHelper* message, void (TFT::*screenSetup)()) noexcept
{
  // Show message and call function only after a timeout
  tft_.fillRect(POPUP_X, POPUP_Y, POPUP_W, POPUP_TITLE_H, colMenuBackColor);
  tft_.fillRect(POPUP_X, POPUP_Y + POPUP_TITLE_H, POPUP_W, POPUP_H, colBackColor);
  tft_.drawRect(POPUP_X, POPUP_Y + POPUP_TITLE_H, POPUP_W, POPUP_H, colMenuBackColor);
  tft_.fillRect(POPUP_BTN_X, POPUP_BTN_Y, POPUP_BTN_W, POPUP_BTN_H, colMenuBackColor);

  int16_t tx, ty;
  uint16_t tw, th;
  tft_.setFont(&FreeSans12pt7b);

  // print title
  tft_.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
  tft_.setTextColor(colMenuFontColor, colMenuBackColor);
  tft_.setCursor(POPUP_X + (POPUP_W - tw) / 2, POPUP_Y + (POPUP_TITLE_H - th) / 2 + BASELINE_MIDDLE);
  tft_.print(title);

  // print button text
  tft_.getTextBounds(F("OK"), 0, 0, &tx, &ty, &tw, &th);
  tft_.setTextColor(colMenuFontColor, colMenuBackColor);
  tft_.setCursor(POPUP_BTN_X + (POPUP_BTN_W - tw) / 2, POPUP_BTN_Y + (POPUP_BTN_H - th) / 2 + BASELINE_MIDDLE);
  tft_.print(F("OK"));

  // print message itself
  tft_.setFont(&FreeSans9pt7b);
  tft_.getTextBounds(F("OK"), 0, 0, &tx, &ty, &tw, &th);
  tft_.setTextColor(colFontColor, colBackColor);
  auto y = POPUP_Y + POPUP_TITLE_H + 10 + BASELINE_SMALL;
  tft_.setCursor(POPUP_X + 10, y);
  auto ptr = reinterpret_cast<const char*>(message);
  while (char c = static_cast<char>(pgm_read_byte(ptr++))) {
    if (c == '\n') {
      y += 10 + th;
      tft_.setCursor(POPUP_X + 10, y);
    } else {
      tft_.print(c);
    }
  }
  //tft_.print(message);

  popup_action_ = screenSetup;
  millis_popup_show_time_ = millis();
}

void TFT::doRestartImpl() noexcept
{
  wdt_disable();
  asm volatile ("jmp 0");
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
  BASELINE_BIGNUMBER = int(h);
  Serial.print (F("Font baseline (big / middle / small): "));
  Serial.print (h);
  Serial.print (F(" / "));
  tft_.setFont(&FreeSans12pt7b);  // Mittlerer Font
  // Baseline bestimmen für kleinen Font
  char Char9[] = "9";
  tft_.getTextBounds(Char9, 0, 0, &x1, &y1, &w, &h);
  BASELINE_MIDDLE = int(h);
  HEIGHT_NUMBER_FIELD = int(h + 1);
  Serial.print (HEIGHT_NUMBER_FIELD);
  Serial.print (F(" / "));
  tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
  // Baseline bestimmen für kleinen Font
  tft_.getTextBounds(CharM, 0, 0, &x1, &y1, &w, &h);
  BASELINE_SMALL = int(h);
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
