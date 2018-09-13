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

// Timing:

/// Interval for updating displayed values (1s).
static constexpr unsigned long INTERVAL_DISPLAY_UPDATE = 1000000;
/// Interval for detecting second menu button press (500ms). At least this time must pass between two touches.
static constexpr unsigned long INTERVAL_MENU_BTN = 500;
/// Interval for returning back to main screen if nothing pressed (1m).
static constexpr unsigned long INTERVAL_TOUCH_TIMEOUT = 60000;
/// Interval for turning off the display (if possible) if nothing pressed (5m).
static constexpr unsigned long INTERVAL_DISPLAY_TIMEOUT = 5 * 60000;
/// If the user doesn't confirm the popup within this time, it will be closed automatically.
static constexpr unsigned long POPUP_TIMEOUT_MS = 10000;
/// If the user doesn't change anything in popup flags within this time, it will be closed automatically.
static constexpr unsigned long POPUP_FLAG_TIMEOUT_MS = 30000;
/// Minimum time in ms to not react on input on screen turned off.
static constexpr unsigned long SCREEN_OFF_MIN_TIME = 2000;
/// Delay for 4s to be able to read display messages.
static constexpr unsigned long STARTUP_DELAY = 4000;

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
#define colInputBackColor           0x31A6 // 20% grau
#define colInputFontColor           0xFFFF

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
static constexpr byte INPUT_FIELD_HEIGHT = 34;
/// First input field Y offset.
static constexpr byte INPUT_FIELD_YOFFSET = 62;

/// Popup width.
static constexpr int POPUP_W = 370;
/// Popup title height.
static constexpr int POPUP_TITLE_H = 30;
/// Popup text area height.
static constexpr int POPUP_H = 150;
/// Popup button width.
static constexpr int POPUP_BTN_W = 60;
/// Popup button height.
static constexpr int POPUP_BTN_H = 30;
/// Popup flag edit button width.
static constexpr int POPUP_FLAG_W = 35;
/// Popup flag edit button spacing.
static constexpr int POPUP_FLAG_SPACING = 10;
/// Popup flag edit button height.
static constexpr int POPUP_FLAG_H = 30;
/// Popup X position.
static constexpr int POPUP_X = (480 - TOUCH_BTN_WIDTH - POPUP_W) / 2;
/// Popup Y position.
static constexpr int POPUP_Y = (320 - POPUP_H - POPUP_TITLE_H) / 2;
/// Popup button X position.
static constexpr int POPUP_BTN_Y = POPUP_Y + POPUP_TITLE_H + POPUP_H - 10 - POPUP_BTN_H;
/// Popup button Y position.
static constexpr int POPUP_BTN_X = (480 - TOUCH_BTN_WIDTH - POPUP_BTN_W) / 2;
/// Popup flags Y position.
static constexpr int POPUP_FLAG_Y = POPUP_BTN_Y - 10 - POPUP_BTN_H;
/// Popup flags X position.
static constexpr int POPUP_FLAG_X = POPUP_X + POPUP_FLAG_SPACING;


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

/// Helper function to update data in range.
template<typename T, typename TD>
static void update_minmax(T& value, TD diff, T min, T max) noexcept
{
  TD sum = TD(value) + diff;
  if (sum < TD(min))
    sum = TD(min);
  else if (sum > TD(max))
    sum = TD(max);
  value = T(sum);
}

/// Helper to format flag array.
void format_flags(char* buf, const __FlashStringHelper* flag_names, uint8_t flag_count, uint8_t flag_name_length, uint16_t flags) noexcept
{
  uint16_t mask = 1;
  const char* src = reinterpret_cast<const char*>(flag_names);
  for (uint8_t i = 0; i < flag_count; ++i, mask <<= 1) {
    for (uint8_t j = 0; j < flag_name_length; ++j) {
      if (mask & flags)
        *buf = char(pgm_read_byte(src));
      else
        *buf = '-';
      ++buf;
      ++src;
    }
  }
  *buf = 0;
}


/******************************************* Seitenverwaltung ********************************************
  Jeder Screen ist durch screen*() Methode aufgesetzt und muss folgende Aktionen tun:
   - Hintergrund vorbereiten
   - Menü vorbereiten via newMenuEntry() Calls, mit Aktionen (z.B., via gotoScreen() um Screen zu wechseln)
   - Falls Screenupdates gebraucht werden, display_update_ setzen (es wird auch initial aufgerufen)
   - Falls screenspezifische Variablen gebraucht werden, initialisieren (in screen_state_)
*/

/// Menu action for a button.
class MenuAction
{
public:
  MenuAction() = default;

  /// Store function object into the menu action.
  template<typename Func>
  MenuAction& operator=(Func&& f) noexcept
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

  /// Clear the function object.
  MenuAction& operator=(decltype(nullptr)) noexcept {
    invoker_ = nullptr;
    return *this;
  }

  /// Check if an action is present.
  explicit operator bool() const noexcept { return invoker_ != nullptr; }

  /// Invoke the menu action.
  void invoke() noexcept {
    if (invoker_)
      invoker_(this);
  }

private:
  /// Invoker for the action.
  void (*invoker_)(MenuAction* m) = nullptr;
  /// Menu action state (function closure).
  char state_[6];
};

/// Base class for all control elements (buttons, etc.).
class TFT::Control
{
public:
  Control(TFT& owner) noexcept : owner_(owner), tft_(owner.getTFT()) {}

protected:
  friend class TFT;

  /*!
   * @brief Initialize the screen or control.
   *
   * This is called after constructing the object to draw initial contents.
   */
  virtual void init() noexcept
  {}

  /*!
   * @brief React to touch input.
   *
   * @param x,y coordinates of input touch.
   * @param time time of the touch in milliseconds.
   * @return @c true, if touch consumed, @c false if not.
   */
  virtual bool touch(int16_t x, int16_t y, unsigned long time) noexcept { return false; }

  /// React to touch input release, passes time in milliseconds.
  virtual void release(unsigned long time) noexcept {}

  /// Get time in milliseconds at which the last touch input was detected.
  unsigned long getLastTouchTime() const noexcept { return owner_.millis_last_touch_; }

  /// Controller to use.
  TFT& owner_;
  /// Reference to low-level TFT object.
  MCUFRIEND_kbv& tft_;
};

/// Control element with menu buttons.
class ControlMenuButtons : public TFT::Control
{
public:
  ControlMenuButtons(TFT& owner) noexcept :
    Control(owner)
  {
    memset(menu_btn_action_, 0, sizeof(menu_btn_action_));
  }

  /*!
   * @brief Set up new menu entry when creating a screen.
   *
   * @param mnuBtn button index.
   * @param mnuTxt button text.
   * @param action action to execute when button pressed.
   */
  template<typename Func>
  void newMenuEntry(byte mnuBtn, const __FlashStringHelper* mnuTxt, Func&& action) noexcept
  {
    if (mnuBtn < 1 || mnuBtn > MENU_BTN_COUNT) {
      Serial.println(F("Trying to set menu action for invalid index"));
      return;
    }
    menu_btn_action_[mnuBtn - 1] = action;
    drawMenuButton(mnuBtn, mnuTxt, colMenuBtnFrame);
  }

protected:
  friend class TFT;

  virtual void init() noexcept override
  {
    Control::init();

    // Menu Hintergrund
    tft_.fillRect(480 - TOUCH_BTN_WIDTH , TOUCH_BTN_YOFFSET, TOUCH_BTN_WIDTH , 320 - TOUCH_BTN_YOFFSET - 20, colMenuBackColor );
    last_highlighed_menu_btn_ = 0;
  }

  virtual bool touch(int16_t x, int16_t y, unsigned long time) noexcept override
  {
    if (Control::touch(x, y, time))
      return true;

    if (time - millis_last_menu_btn_press_ > INTERVAL_MENU_BTN &&
        x >= 480 - TOUCH_BTN_WIDTH) {
      // we are in menu area and there was enough time between touches
      uint8_t button = uint8_t((y - TOUCH_BTN_YOFFSET) / TOUCH_BTN_HEIGHT);
      if (button < MENU_BTN_COUNT) {
        // touched a menu button
        millis_last_menu_btn_press_ = time;

        if (KWLConfig::serialDebugDisplay) {
          Serial.print(F("TFT: menu button touched: "));
          Serial.println(button);
        }

        if (menu_btn_action_[button]) {
          // run appropriate menu action
          setMenuBorder(button + 1);
          menu_btn_action_[button].invoke();
        }
      }
      return true;
    }
    if (time - millis_last_menu_btn_press_ > INTERVAL_MENU_BTN - 100)
      setMenuBorder(0); // clear button highlight after timeout

    return (x >= 480 - TOUCH_BTN_WIDTH);
  }

  virtual void release(unsigned long time) noexcept override
  {
    setMenuBorder(0);
  }

private:
  /// Print a menu entry with given color for the frame.
  void drawMenuButton(byte mnuBtn, const __FlashStringHelper* mnuTxt, uint16_t colFrame) noexcept
  {
    // ohne mnuTxt wird vom Button nur der Rand gezeichnet
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

  /// Draw menu border around given menu button.
  void setMenuBorder(byte menuBtn) noexcept
  {
    if (menuBtn == last_highlighed_menu_btn_)
      return;

    if (last_highlighed_menu_btn_)
      drawMenuButton(last_highlighed_menu_btn_, F(""), colMenuBtnFrame);
    if (menuBtn)
      drawMenuButton(menuBtn, F(""), colMenuBtnFrameHL);

    last_highlighed_menu_btn_ = menuBtn;
  }

  /// Maximum menu button count.
  static constexpr uint8_t MENU_BTN_COUNT = 6;
  /// Last menu button, which was highlighted.
  byte last_highlighed_menu_btn_ = 0;
  /// Time at which the menu button was last pressed.
  unsigned long millis_last_menu_btn_press_ = 0;
  /// Menu actions.
  MenuAction menu_btn_action_[MENU_BTN_COUNT];
};

class ScreenMain;
class ScreenSaver;
class ScreenAdditionalSensors;
class ScreenSetup;
class ScreenSetup2;
class ScreenSetupFan;
class ScreenSetupBypass;
class ScreenSetupIPAddress;
class ScreenSetupTime;
class ScreenSetupAntifreeze;
class ScreenSetupProgram;
class ScreenSetupFactoryDefaults;

/// Base class for all screens.
class TFT::Screen : public TFT::Control
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 0;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID;

  /// Define control type to create, if any.
  using control_type = TFT::Control;

  /*!
   * @brief Construct a screen.
   *
   * The constructor typically stores values needed for interactive screens in
   * screen state variables.
   *
   * @param owner owner of the screen.
   */
  explicit Screen(TFT& owner) noexcept : Control(owner) {}

  template<typename Control>
  Control& getControls() noexcept {
    return *static_cast<Control*>(owner_.current_control_);
  }

protected:
  friend class TFT;

  /*!
   * @brief Check whether last screen displayed was of the specified class.
   *
   * This can be used, e.g., to skip drawing common parts of the screen.
   */
  template<typename ScreenClass>
  inline bool lastScreenWas() noexcept
  {
    return owner_.lastScreenWas<ScreenClass>();
  }

  /*!
   * @brief Change to the specified screen class.
   *
   * @tparam ScreenClass screen class to display.
   */
  template<typename ScreenClass>
  void gotoScreen() noexcept
  {
    owner_.gotoScreen<ScreenClass>();
  }

  /// Get main controller object.
  inline KWLControl& getControl() const noexcept { return owner_.getControl(); }

  /// Update the screen periodically, if needed.
  virtual void update() noexcept;

  /// Check whether the screen contains changed values.
  virtual bool is_changed() noexcept { return false; }
};

/// Screensaver with black screen.
class ScreenSaver : public TFT::Screen
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 1;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | Screen::IDS;

  explicit ScreenSaver(TFT& owner) noexcept : Screen(owner) {}

protected:
  virtual void init() noexcept override
  {
    Screen::init();

    // make screen black
    if (KWLConfig::serialDebugDisplay)
      Serial.println(F("TFT: Display off"));
    // since we can't really turn off the display, make it at least black
    tft_.fillScreen(TFT_BLACK);

    millis_screen_blank_ = millis();
  }

  virtual void update() noexcept override
  {
    // NOTE: no Screen::update(), since it would timeout back to main screen

    // move last touch time in case of long no-touch, since otherwise
    // we'd run into overflow issues and screen wouldn't wake up after
    // some time
    auto time = millis();
    if (time - millis_screen_blank_ > SCREEN_OFF_MIN_TIME)
      millis_screen_blank_ = time - SCREEN_OFF_MIN_TIME;
  }

  virtual bool touch(int16_t x, int16_t y, unsigned long time) noexcept override
  {
    if (Screen::touch(x, y, time))
      return true;

    if (time - millis_screen_blank_ >= SCREEN_OFF_MIN_TIME) {
      if (KWLConfig::serialDebugDisplay)
        Serial.println(F("TFT: Display on"));
      // now redraw everything
      gotoScreen<ScreenMain>();
    }
    return true;
  }

private:
  /// Time at which the screen went blank.
  unsigned long millis_screen_blank_;
};

/// Screen with global header on top.
class ScreenWithHeader : public TFT::Screen
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 2;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | Screen::IDS;

  explicit ScreenWithHeader(TFT& owner) noexcept : Screen(owner) {}

protected:
  struct PopupFlagsState
  {
    /// Pointer to flags to be edited.
    uint8_t* flags_;
    /// Count of flags.
    uint8_t flag_count_;
    /// Length of one flag name.
    uint8_t flag_name_length_;
    /// Flag names (flag_count_ * flag_name_length_ characters).
    const __FlashStringHelper* flag_names_;
  };

  /// Specify space to be left for controls on this screen.
  void setControlWidth(uint8_t width) noexcept { control_width_ = width; }

  virtual void init() noexcept override
  {
    Screen::init();

    if (!lastScreenWas<ScreenWithHeader>()) {
      // draw the header
      static constexpr auto VERSION = KWLConfig::VersionString;
      tft_.fillRect(0, 0, 480, 30, colBackColor);
      tft_.setCursor(140, 0 + BASELINE_SMALL);
      tft_.setFont(&FreeSans9pt7b);
      tft_.setTextColor(colFontColor);
      tft_.setTextSize(1);
      tft_.print(F(" * Pluggit AP 300 * "));
      tft_.setCursor(420, 0 + BASELINE_SMALL);
      tft_.print(VERSION.load());
    }

    // clear screen contents
    tft_.fillRect(0, 30, 480 - control_width_, 270, colBackColor);

    if (!lastScreenWas<ScreenWithHeader>()) {
      // clear status line
      tft_.fillRect(0, 300, 480, 20, colBackColor);
    }

    force_display_update_ = true;
    last_h_ = last_m_ = 255;
  }

  virtual void update() noexcept override
  {
    if (popup_action_) {
      auto timeout = POPUP_TIMEOUT_MS;
      if (popup_flags_)
        timeout = POPUP_FLAG_TIMEOUT_MS;
      if (millis() - millis_popup_show_time_ > timeout) {
        // popup timed out, do the default action
        if (KWLConfig::serialDebugDisplay)
          Serial.println(F("TFT: Popup timed out"));
        auto tmp = popup_action_;
        popup_action_ = nullptr;
        (this->*tmp)();
        return;
      }
    }

    auto& control = getControl();
    auto& net = control.getNetworkClient();

    // Netzwerkverbindung anzeigen
    tft_.setCursor(20, 0 + BASELINE_MIDDLE);
    tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
    if (!net.isLANOk()) {
      if (last_lan_ok_ != net.isLANOk() || force_display_update_) {
        last_lan_ok_ = net.isLANOk();
        tft_.fillRect(10, 0, 120, 20, colErrorBackColor);
        tft_.setTextColor(colErrorFontColor);
        tft_.print(F("ERR LAN"));
      }
    } else if (!net.isMQTTOk()) {
      if (last_mqtt_ok_ != net.isMQTTOk() || force_display_update_) {
        last_mqtt_ok_ = net.isMQTTOk();
        tft_.fillRect(10, 0, 120, 20, colErrorBackColor);
        tft_.setTextColor(colErrorFontColor );
        tft_.print(F("ERR MQTT"));
      }
    } else {
      last_mqtt_ok_ = true;
      last_lan_ok_ = true;
      if (control.getNTP().hasTime()) {
        auto time = control.getNTP().currentTimeHMS(
              control.getPersistentConfig().getTimezoneMin() * 60,
              control.getPersistentConfig().getDST());
        if (time.m != last_m_ || time.h != last_h_ || force_display_update_) {
          tft_.fillRect(10, 0, 120, 20, colBackColor);
          char buffer[6];
          time.writeHM(buffer);
          buffer[5] = 0;
          tft_.setTextColor(colFontColor);
          tft_.print(buffer);
          last_h_ = time.h;
          last_m_ = time.m;
        }
      } else if (last_h_ != 255 || last_m_ != 0 || force_display_update_) {
        last_h_ = 255;
        last_m_ = 0;
        tft_.fillRect(10, 0, 120, 20, colErrorBackColor);
        tft_.setTextColor(colErrorFontColor);
        tft_.print(F("ERR NTP"));
      }
    }

    // Zeige Status
    unsigned errors = control.getErrors() & ~(KWLControl::ERROR_BIT_CRASH | KWLControl::ERROR_BIT_NTP);
    unsigned infos = control.getInfos();
    if (errors != 0) {
      if (errors != last_error_bits_) {
        // Neuer Fehler
        tft_.fillRect(0, 300, 480, 21, colErrorBackColor);
        tft_.setTextColor(colErrorFontColor );
        tft_.setFont(&FreeSans9pt7b);  // Kleiner Font
        tft_.setCursor(18, 301 + BASELINE_SMALL);
        char buffer[80];
        control.errorsToString(buffer, sizeof(buffer));
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
        control.infosToString(buffer, sizeof(buffer));
        tft_.print(buffer);
        last_info_bits_ = infos;
        tft_.setFont(&FreeSans12pt7b);  // Mittlerer Font
      }
    }
    else if (last_error_bits_ != 0 || last_info_bits_ != 0 || force_display_update_) {
      // Keine Fehler oder Infos
      tft_.fillRect(0, 300, 480, 21, colBackColor);
      last_error_bits_ = 0;
      last_info_bits_ = 0;
    }

    force_display_update_ = false;

    Screen::update();
  }

  /*!
   * @brief Set flags to edit in popup and draw them.
   *
   * @param flags flags state to use.
   */
  void setPopupFlags(PopupFlagsState& flags) noexcept
  {
    popup_flags_ = &flags;
    for (uint8_t i = 0; i < flags.flag_count_; ++i)
      drawPopupFlag(i);
  }

  /*!
   * @brief Show popup and go to specified screen afterwards.
   *
   * @param title popup window title.
   * @param message message to show in the window.
   * @tparam NextScreen screen to display after popup closed.
   */
  template<typename NextScreen>
  void doPopup(const __FlashStringHelper* title, const __FlashStringHelper* message) noexcept
  {
    doPopupImpl(title, message, &Screen::gotoScreen<NextScreen>);
  }

  /*!
   * @brief Restart the controller after displaying the message.
   *
   * @param title popup window title.
   * @param message message to show in the window.
   */
  void doRestart(const __FlashStringHelper* title, const __FlashStringHelper* message) noexcept
  {
    doPopupImpl(title, message, static_cast<void(Screen::*)()>(&ScreenWithHeader::doRestartImpl));
  }

  virtual bool touch(int16_t x, int16_t y, unsigned long time) noexcept override
  {
    if (Screen::touch(x, y, time))
      return true;

    if (!popup_action_)
      return false;

    // popup is active, it has focus
    if (popup_flags_ &&
        y >= POPUP_FLAG_Y - 10 && y <= POPUP_FLAG_Y + POPUP_FLAG_H + 5 &&
        x >= POPUP_FLAG_X - POPUP_FLAG_SPACING / 2) {
      // check for editable flags
      uint8_t idx = uint8_t((x - POPUP_FLAG_X + POPUP_FLAG_SPACING / 2) / (POPUP_FLAG_W + POPUP_FLAG_SPACING));
      if (idx < popup_flags_->flag_count_) {
        // flip the flag
        if (time - millis_popup_show_time_ >= INTERVAL_MENU_BTN) {
          if (KWLConfig::serialDebugDisplay) {
            Serial.print(F("TFT: Popup flag touched: "));
            Serial.println(idx);
          }
          *popup_flags_->flags_ ^= uint8_t(1U << idx);
          drawPopupFlag(idx);
          millis_popup_show_time_ = time;
        }
        return true;
      }
    }

    // check for OK button
    if (x >= POPUP_BTN_X - 20 && x < POPUP_BTN_X + POPUP_BTN_W + 20 &&
        y >= POPUP_BTN_Y - 20 && y < POPUP_BTN_Y + POPUP_BTN_H + 20) {
      // popup button hit, call action
      auto tmp = popup_action_;
      popup_action_ = nullptr;
      if (KWLConfig::serialDebugDisplay)
        Serial.println(F("TFT: Popup OK button touched"));
      (this->*tmp)();
      return true;
    }

    // always eat touch input on popup
    return true;
  }

private:
  /// Implementation for doPopup()
  void doPopupImpl(const __FlashStringHelper* title, const __FlashStringHelper* message, void (Screen::*action)()) noexcept
  {
    millis_popup_show_time_ = millis();

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
    tft_.setCursor(POPUP_X + (POPUP_W - tw) / 2, int16_t(POPUP_Y + (POPUP_TITLE_H - th) / 2) + BASELINE_MIDDLE);
    tft_.print(title);

    // print button text
    tft_.getTextBounds(F("OK"), 0, 0, &tx, &ty, &tw, &th);
    tft_.setTextColor(colMenuFontColor, colMenuBackColor);
    tft_.setCursor(POPUP_BTN_X + (POPUP_BTN_W - tw) / 2, int16_t(POPUP_BTN_Y + (POPUP_BTN_H - th) / 2) + BASELINE_MIDDLE);
    tft_.print(F("OK"));

    // print message, if any
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

    popup_action_ = action;
    popup_flags_ = nullptr;
  }

  /// Immediately restart the controller.
  void doRestartImpl() noexcept
  {
    wdt_disable();
    asm volatile ("jmp 0");
  }

  /*!
   * @brief Draw one popup flag.
   *
   * @param idx flag index (0-based).
   */
  void drawPopupFlag(uint8_t idx) noexcept
  {
    auto x = POPUP_FLAG_X + idx * (POPUP_FLAG_W + POPUP_FLAG_SPACING);
    auto y = POPUP_FLAG_Y;
    bool highlight = (*popup_flags_->flags_ & (1U << idx)) != 0;

    char flag_name[8];
    const char* src = reinterpret_cast<const char*>(popup_flags_->flag_names_) + idx * popup_flags_->flag_name_length_;
    char* dst = flag_name;
    for (uint8_t i = 0; i < popup_flags_->flag_name_length_; ++i)
      *dst++ = char(pgm_read_byte(src++));
    *dst = 0;
    tft_.setFont(&FreeSans9pt7b);
    int16_t tx, ty;
    uint16_t tw, th;
    tft_.getTextBounds(flag_name, 0, 0, &tx, &ty, &tw, &th);

    if (highlight) {
      tft_.fillRect(x, y, POPUP_FLAG_W, POPUP_FLAG_H, colMenuBackColor);
      tft_.setTextColor(colMenuFontColor, colMenuBackColor);
    } else {
      tft_.fillRect(x + 1, y + 1, POPUP_FLAG_W - 2, POPUP_FLAG_H - 2, colBackColor);
      tft_.drawRect(x, y, POPUP_FLAG_W, POPUP_FLAG_H, colMenuBackColor);
      tft_.setTextColor(colFontColor, colBackColor);
    }
    tft_.setCursor(x + int16_t(POPUP_FLAG_W - tw) / 2, (y + int16_t(POPUP_FLAG_H - th) / 2) + BASELINE_SMALL);
    tft_.print(flag_name);
  }

  /// Time at which the popup started showing.
  unsigned long millis_popup_show_time_ = 0;
  /// Action for popup timeout and OK button.
  void (Screen::*popup_action_)() = nullptr;
  /// Popup flags to use.
  PopupFlagsState* popup_flags_ = nullptr;

  /// Last shown error bits.
  unsigned last_error_bits_ = 0;
  /// Last shown info bits.
  unsigned last_info_bits_  = 0;
  /// Last shown network state.
  bool last_lan_ok_  = true;
  /// Last shown MQTT state.
  bool last_mqtt_ok_ = true;
  /// Last shown hour/minute.
  uint8_t last_h_ = 255, last_m_ = 255;
  /// Force display update also of not changed values.
  bool force_display_update_ = true;
  /// Widht of additional controls on screen's right side.
  uint8_t control_width_ = TOUCH_BTN_WIDTH;
};

/// Initial screen showing boot messages.
class ScreenInit : public ScreenWithHeader
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 3;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithHeader::IDS;

  explicit ScreenInit(TFT& owner) noexcept :
    ScreenWithHeader(owner)
  {
    setControlWidth(0);
  }

protected:
  virtual void init() noexcept override
  {
    ScreenWithHeader::init();

    // set position for boot messages
    tft_.setCursor(0, 30 + BASELINE_SMALL);
  }

  virtual void update() noexcept override
  {
    ++update_count_;

    // NOTE: intentionally no update on status yet

    auto time = millis();
    if (update_count_ == 2) {
      // NOTE: we get first update when setting up the screen, second on TFT::begin()
      // after boot messages printed.
      first_update_ = time;
    } else if (update_count_ > 2 && time - first_update_ > STARTUP_DELAY - 100) {
      // timeout, switch to main screen
      gotoScreen<ScreenMain>();
    }
  }

private:
  uint8_t update_count_ = 0;
  unsigned long first_update_ = 0;
};

/// Screen with header and small title bar.
class ScreenWithSmallTitle : public ScreenWithHeader
{
protected:
  ScreenWithSmallTitle(TFT& owner, const __FlashStringHelper* title) noexcept :
    ScreenWithHeader(owner), title_(title)
  {}

  virtual void init() noexcept override
  {
    ScreenWithHeader::init();

    // Show screen title bar with screen title
    int16_t  x1, y1;
    uint16_t w, h;

    tft_.setFont(&FreeSans9pt7b);
    tft_.setTextSize(1);
    tft_.getTextBounds(title_, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor((480 - TOUCH_BTN_WIDTH) / 2 - w / 2, 30 + BASELINE_SMALL);
    tft_.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor);
    tft_.fillRect(0, 30, 480 - TOUCH_BTN_WIDTH, 20, colWindowTitleBackColor);
    tft_.print(title_);
    tft_.setTextColor(colFontColor, colBackColor);
  }

private:
  const __FlashStringHelper* title_;
};

/// Screen with header and big title bar.
class ScreenWithBigTitle : public ScreenWithHeader
{
protected:
  ScreenWithBigTitle(TFT& owner, const __FlashStringHelper* title) noexcept :
    ScreenWithHeader(owner), title_(title)
  {}

  virtual void init() noexcept override
  {
    ScreenWithHeader::init();

    // Show screen title bar with screen title
    int16_t  x1, y1;
    uint16_t w, h;

    tft_.setFont(&FreeSans12pt7b);
    tft_.setTextSize(1);
    tft_.getTextBounds(title_, 0, 0, &x1, &y1, &w, &h);
    tft_.setCursor((480 - TOUCH_BTN_WIDTH) / 2 - w / 2, 33 + BASELINE_MIDDLE);
    tft_.setTextColor(colWindowTitleFontColor, colWindowTitleBackColor);
    tft_.fillRect(0, 30, 480 - TOUCH_BTN_WIDTH, 30, colWindowTitleBackColor);
    tft_.print(title_);
    tft_.setTextColor(colFontColor, colBackColor);
  }

private:
  const __FlashStringHelper* title_;
};

/// Screen with input fields.
template<typename ScreenBase>
class ScreenWithInput : public ScreenBase
{
protected:
  ScreenWithInput(TFT& owner, const __FlashStringHelper* title) noexcept :
    ScreenBase(owner, title)
  {
    for (uint8_t i = 0; i < INPUT_ROW_COUNT; ++i)
      input_active_[i] = 0;
    setupInputFieldColumns();
  }

  void init() noexcept
  {
    ScreenBase::init();
    input_current_col_ = input_current_row_ = 0;
    input_highlight_ = false;
  }

  /*!
   * @brief Set up input field column positions.
   *
   * This method is called with default values when setting up input action, if any.
   *
   * If a non-uniform spacing is desired, set up input_x_ and input_w_ values appropriately.
   *
   * @param left position of the leftmost column.
   * @param width width of one column.
   * @param spacing spacing between columns.
   */
  void setupInputFieldColumns(unsigned left = 180, unsigned width = 50, unsigned spacing = 10) noexcept
  {
    input_x_ = int(left);
    input_spacing_ = int(spacing);
    for (uint8_t i = 0; i < INPUT_ROW_COUNT; ++i)
      input_w_[i] = int(width);
  }

  /*!
   * @brief Set up input field column width for a specified row.
   *
   * This overrides row defaults set by setupInputFieldColumns().
   *
   * @param row Row for which to set the new width.
   * @param width Column width for the row.
   */
  void setupInputFieldColumnWidth(uint8_t row, unsigned width) noexcept
  {
    if (row > 0 && row <= INPUT_ROW_COUNT)
      input_w_[row - 1] = int(width);
  }

  /*!
   * @brief Set up input field row.
   *
   * @param row input field row.
   * @param count count of input fields.
   * @param header input field header to print.
   * @param separator if set, this will be printed in spaces between input columns (e.g., '.' for IP address).
   */
  void setupInputFieldRow(uint8_t row, uint8_t count, const char* header, const __FlashStringHelper* separator = nullptr) noexcept
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
      this->tft_.getTextBounds(separator, 0, 0, &x1, &y1, &tw, &th);
      auto x = input_x_;
      auto w = input_w_[row];
      for (uint8_t i = 0; i < count - 1; ++i, x += w + input_spacing_) {
        int x1 = x + w;
        int x2 = x1 + input_spacing_ - 4;
        this->tft_.setCursor((x1 + x2 - int(tw)) / 2, y + 12 + BASELINE_SMALL);
        this->tft_.print(separator);
      }
    }

    // draw input fields
    input_current_row_ = row + 1;
    for (uint8_t i = 0; i < count; ++i) {
      input_current_col_ = i;
      input_field_draw(input_current_row_, input_current_col_);
    }
    input_current_row_ = input_current_col_ = 0;

    // print row header
    this->tft_.setTextColor(colFontColor, colBackColor);
    this->tft_.setFont(&FreeSans12pt7b);
    this->tft_.setCursor(18, y + 12 + BASELINE_SMALL);
    this->tft_.print(header);
  }

  /*!
   * @brief Set up input field row.
   *
   * @param row input field row.
   * @param count count of input fields.
   * @param header input field header to print.
   * @param separator if set, this will be printed in spaces between input columns (e.g., '.' for IP address).
   */
  void setupInputFieldRow(uint8_t row, uint8_t count, const __FlashStringHelper* header, const __FlashStringHelper* separator = nullptr) noexcept
  {
    setupInputFieldRow(row, count, "", separator);
    this->tft_.print(header);
  }

  /*!
   * @brief Reset active input field, if any.
   */
  void resetInput() noexcept
  {
    if (input_current_row_) {
      input_highlight_ = false;
      input_field_leave(input_current_row_, input_current_col_);
      input_highlight_ = false;
      input_field_draw(input_current_row_, input_current_col_);
      input_current_col_ = input_current_row_ = 0;
    }
  }

  /// Update current input field on screen.
  void updateCurrentInputField() noexcept
  {
    input_field_draw(input_current_row_, input_current_col_);
  }

  /// Update specified input field on screen.
  void updateInputField(uint8_t row, uint8_t col) noexcept
  {
    if (row == input_current_row_ && col == input_current_col_) {
      input_field_draw(input_current_row_, input_current_col_);
    } else {
      auto old_row = input_current_row_;
      auto old_col = input_current_col_;
      auto old_hl = input_highlight_;
      input_current_row_ = row;
      input_current_col_ = col;
      input_highlight_ = false;
      input_field_draw(row, col);
      input_current_row_ = old_row;
      input_current_col_ = old_col;
      input_highlight_ = old_hl;
    }
  }

  /// Update all input fields on screen (except current).
  void updateAllInputFields() noexcept
  {
    auto cur_row = input_current_row_;
    auto cur_col = input_current_col_;
    auto cur_high = input_highlight_;
    input_highlight_ = false;

    for (uint8_t i = 1; i <= INPUT_ROW_COUNT; ++i) {
      input_current_row_ = i;
      auto row_mask = input_active_[i - 1];
      for (uint8_t j = 0, mask = 1; j < 8; ++j, mask <<= 1) {
        if ((row_mask & mask) != 0 && ((j != cur_col) || (i != cur_row))) {
          input_current_col_ = j;
          input_field_draw(input_current_row_, input_current_col_);
        }
      }
    }

    input_current_row_ = cur_row;
    input_current_col_ = cur_col;
    input_highlight_ = cur_high;
  }

  /*!
   * @brief Draw the contents of an input field, e.g., upon screen setup.
   *
   * @param row,col row and column of the input field.
   * @param text field value.
   * @param highlight set to @c true to draw the field highlighted.
   * @param right_align if set to @c true, align on the right side, else on the left side.
   */
  void drawInputField(uint8_t row, uint8_t col, const char* text, bool highlight = false, bool right_align = true) noexcept
  {
    if (row == 0 || row > INPUT_ROW_COUNT)
      return; // error
    --row;

    int w = input_w_[row];
    int x = input_x_ + col * (w + input_spacing_);
    int y = INPUT_FIELD_YOFFSET + 1 + INPUT_FIELD_HEIGHT * row;

    if (highlight) {
      // Highlighted field
      this->tft_.fillRect(x, y, w, INPUT_FIELD_HEIGHT - 2, colMenuBackColor);
      this->tft_.setTextColor(colMenuFontColor, colMenuBackColor);
    } else {
      // Normal field
      this->tft_.fillRect(x, y, w, INPUT_FIELD_HEIGHT - 2, colInputBackColor);
      this->tft_.setTextColor(colInputFontColor, colInputBackColor);
    }

    int16_t  x1, y1;
    uint16_t tw, th;

    this->tft_.setFont(&FreeSans12pt7b);
    this->tft_.getTextBounds(const_cast<char*>(text), 0, 0, &x1, &y1, &tw, &th);
    if (right_align)
      x += w - 10 - int(tw);
    this->tft_.setCursor(x + 5, y + 12 + BASELINE_SMALL);
    this->tft_.print(text);
  }

  /*!
   * @brief Draw the contents of an input field, e.g., upon screen setup.
   *
   * @param row,col row and column of the input field.
   * @param text field value.
   * @param highlight set to @c true to draw the field highlighted.
   * @param right_align if set to @c true, align on the right side, else on the left side.
   */
  void drawInputField(uint8_t row, uint8_t col, const __FlashStringHelper* text, bool highlight = false, bool right_align = true) noexcept
  {
    char* buffer = reinterpret_cast<char*>(alloca(strlen_P(reinterpret_cast<const char*>(text)) + 1));
    strcpy_P(buffer, reinterpret_cast<const char*>(text));
    drawInputField(row, col, buffer, highlight, right_align);
  }

  /*!
   * @brief Draw the contents of the current input field, e.g., upon value update.
   *
   * @param text field value.
   * @param right_align if set to @c true, align on the right side, else on the left side.
   */
  void drawCurrentInputField(const char* text, bool right_align = true) noexcept
  {
    drawInputField(input_current_row_, input_current_col_, text, input_highlight_, right_align);
  }

  /*!
   * @brief Draw the contents of the current input field, e.g., upon value update.
   *
   * @param text field value.
   * @param right_align if set to @c true, align on the right side, else on the left side.
   */
  void drawCurrentInputField(const __FlashStringHelper* text, bool right_align = true) noexcept
  {
    drawInputField(input_current_row_, input_current_col_, text, input_highlight_, right_align);
  }

  /// Get currently-selected row.
  uint8_t getCurrentRow() const noexcept { return input_current_row_; }

  /// Get currently-selected column.
  uint8_t getCurrentColumn() const noexcept { return input_current_col_; }

  virtual bool touch(int16_t x, int16_t y, unsigned long time) noexcept override
  {
    if (ScreenBase::touch(x, y, time))
      return true;

    auto row = uint8_t((y - INPUT_FIELD_YOFFSET) / INPUT_FIELD_HEIGHT);
    if (row < INPUT_ROW_COUNT && input_active_[row]) {
      // potentially touched an input field
      auto mask = input_active_[row];
      auto w = input_w_[row] + input_spacing_;
      auto cx = input_x_ + input_spacing_ / 2;
      auto col = uint8_t((x - cx) / w);
      if (col < INPUT_COL_COUNT && (mask & (uint8_t(1) << col)) != 0) {
        // active input field
        ++row;
        if (KWLConfig::serialDebugDisplay) {
          Serial.print(F("TFT: Input field touched: row="));
          Serial.print(row);
          Serial.print(F(", col="));
          Serial.println(col);
        }
        if (input_current_row_ != row || input_current_col_ != col) {
          if (input_current_row_) {
            input_highlight_ = false;
            input_field_leave(input_current_row_, input_current_col_);
            input_highlight_ = false;
            input_field_draw(input_current_row_, input_current_col_);
          }
          input_current_col_ = col;
          input_current_row_ = row;
          input_highlight_ = false;
          input_field_enter(row, col);
          input_highlight_ = true;
          input_field_draw(row, col);
        }
        return true;
      }
    }
    return false;
  }

private:
  /// Override to add action when a field is being entered.
  virtual void input_field_enter(uint8_t row, uint8_t col) noexcept {}
  /// Override to add action when a field is being exited.
  virtual void input_field_leave(uint8_t row, uint8_t col) noexcept {}
  /// Override to format field text and draw it.
  virtual void input_field_draw(uint8_t row, uint8_t col) noexcept = 0;

  /// Maximum # of input fields per row. Y positions and height is given by menu buttons.
  static constexpr uint8_t INPUT_COL_COUNT = 8;
  /// Maximum # of input field rows.
  static constexpr uint8_t INPUT_ROW_COUNT = 7;
  /// First input field X coordinates.
  int input_x_ = 0;
  /// Spacing between input fields.
  int input_spacing_ = 0;
  /// Input field width for individual rows.
  int input_w_[INPUT_ROW_COUNT];
  /// Bitmask of active input fields for individual menu rows.
  uint8_t input_active_[INPUT_ROW_COUNT];
  /// Currently selected input row (1-based, like menus).
  uint8_t input_current_row_ = 0;
  /// Currently selected input column (0-based).
  uint8_t input_current_col_ = 0;
  /// Highlight to use when drawing a field.
  bool input_highlight_ = false;
};

/// Main screen displaying current status.
class ScreenMain final : public ScreenWithSmallTitle
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 4;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithSmallTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenMain(TFT& owner) noexcept : ScreenWithSmallTitle(owner, F("Lueftungsstufe")) {}

protected:
  virtual void init() noexcept override
  {
    ScreenWithSmallTitle::init();

    tft_.setFont(&FreeSans9pt7b);

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

    auto& ctrl = getControls<control_type>();
    ctrl.newMenuEntry(1, F("->"),
      [this]() noexcept {
        gotoScreen<ScreenSetup>();
      }
    );
    ctrl.newMenuEntry(2, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenAdditionalSensors>();
      }
    );
    ctrl.newMenuEntry(3, F("+"),
      [this]() noexcept {
        auto& fan = getControl().getFanControl();
        if (fan.getVentilationMode() < int(KWLConfig::StandardModeCnt - 1)) {
          fan.setVentilationMode(fan.getVentilationMode() + 1);
          update();
        }
      }
    );
    ctrl.newMenuEntry(4, F("-"),
      [this]() noexcept {
        auto& fan = getControl().getFanControl();
        if (fan.getVentilationMode() > 0) {
          fan.setVentilationMode(fan.getVentilationMode() - 1);
          update();
        }
      }
    );
    ctrl.newMenuEntry(6, F("Off"),
      [this]() noexcept {
        gotoScreen<ScreenSaver>();
      }
    );
  }

  virtual void update() noexcept override
  {
    ScreenWithSmallTitle::update();

    char buffer[8];
    auto& fan = getControl().getFanControl();
    auto& temp = getControl().getTempSensors();

    auto currentMode = fan.getVentilationMode();
    if (kwl_mode_ != currentMode) {
      // KWL Mode
      tft_.setFont(&FreeSansBold24pt7b);
      tft_.setTextColor(colFontColor, colBackColor);
      tft_.setTextSize(2);

      tft_.setCursor(200, 55 + 2 * BASELINE_BIGNUMBER);
      snprintf(buffer, sizeof(buffer), "%-1i", currentMode);
      tft_.fillRect(200, 55, 60, 80, colBackColor);
      tft_.print(buffer);
      kwl_mode_ = currentMode;
      tft_.setTextSize(1);
    }
    tft_.setFont(&FreeSans12pt7b);
    tft_.setTextColor(colFontColor, colBackColor);

    int16_t  x1, y1;
    uint16_t w, h;

    // Speed Fan1
    int speed1 = int(fan.getFan1().getSpeed());
    if (abs(tacho_fan1_ - speed1) > 10) {
      tacho_fan1_ = speed1;
      tft_.fillRect(280, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      snprintf(buffer, sizeof(buffer), "%5i", speed1);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(340 - int(w), 192 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
    // Speed Fan2
    int speed2 = int(fan.getFan2().getSpeed());
    if (abs(tacho_fan2_ - speed2) > 10) {
      tacho_fan2_ = speed2;
      snprintf(buffer, sizeof(buffer), "%5i", speed2);
      // Debug einkommentieren
      // tft_.fillRect(280, 218, 60, HEIGHT_NUMBER_FIELD, colWindowTitle);
      tft_.fillRect(280, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(340 - int(w), 218 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
    // T1
    if (abs(t1_ - temp.get_t1_outside()) > 0.5) {
      t1_ = temp.get_t1_outside();
      tft_.fillRect(160, 166, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(t1_, 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - int(w), 166 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
    // T2
    if (abs(t2_ - temp.get_t2_inlet()) > 0.5) {
      t2_ = temp.get_t2_inlet();
      tft_.fillRect(160, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(t2_, 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - int(w), 192 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
    // T3
    if (abs(t3_ - temp.get_t3_outlet()) > 0.5) {
      t3_ = temp.get_t3_outlet();
      tft_.fillRect(160, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(t3_, 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - int(w), 218 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
    // T4
    if (abs(t4_ - temp.get_t4_exhaust()) > 0.5) {
      t4_ = temp.get_t4_exhaust();
      tft_.fillRect(160, 244, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      dtostrf(t4_, 5, 1, buffer);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - int(w), 244 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
    // Wirkungsgrad
    if (abs(efficiency_ - temp.getEfficiency()) > 1) {
      efficiency_ = temp.getEfficiency();
      tft_.fillRect(160, 270, 80, HEIGHT_NUMBER_FIELD, colBackColor);
      snprintf(buffer, sizeof(buffer), "%5d %%", efficiency_);
      tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      tft_.setCursor(240 - int(w), 270 + BASELINE_MIDDLE);
      tft_.print(buffer);
    }
  }

private:
  // Last displayed values
  // Set to invalid values to force refresh on first display update
  int tacho_fan1_ = -1;
  int tacho_fan2_ = -1;
  int kwl_mode_ = -1;
  int efficiency_ = -1;
  double t1_ = -1000, t2_ = -1000, t3_ = -1000, t4_ = -1000;
};

/// Screen displaying additional sensors.
class ScreenAdditionalSensors : public ScreenWithSmallTitle
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 5;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithSmallTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenAdditionalSensors(TFT& owner) noexcept : ScreenWithSmallTitle(owner, F("Weitere Sensorwerte")) {}

protected:
  virtual void init() noexcept override
  {
    ScreenWithSmallTitle::init();

    auto& control = getControl();
    auto& sensors = control.getAdditionalSensors();

    tft_.setFont(&FreeSans12pt7b);

    if (sensors.hasDHT1()) {
      tft_.setCursor(18, 166 + BASELINE_MIDDLE);
      tft_.print(F("DHT1"));
    }

    if (sensors.hasDHT2()) {
      tft_.setCursor(18, 192 + BASELINE_MIDDLE);
      tft_.print(F("DHT2"));
    }

    if (sensors.hasCO2()) {
      tft_.setCursor(18, 218 + BASELINE_MIDDLE);
      tft_.print(F("CO2"));
    }
    if (sensors.hasVOC()) {
      tft_.setCursor(18, 244 + BASELINE_MIDDLE);
      tft_.print(F("VOC"));
    }

    auto& ctrl = getControls<control_type>();
    ctrl.newMenuEntry(1, F("->"),
      [this]() noexcept {
        gotoScreen<ScreenMain>();
      }
    );
  }

  virtual void update() noexcept override
  {
    tft_.setFont(&FreeSans12pt7b);
    tft_.setTextColor(colFontColor, colBackColor);

    auto& control = getControl();
    auto& sensors = control.getAdditionalSensors();

    char buffer[16];
    int16_t  x1, y1;
    uint16_t w, h;

    // DHT 1
    if (sensors.hasDHT1()) {
      if (abs(double(dht1_temp_ - sensors.getDHT1Temp())) > 0.1) {
        dht1_temp_ = sensors.getDHT1Temp();
        tft_.fillRect(160, 166, 80, HEIGHT_NUMBER_FIELD, colBackColor);
        dtostrf(double(dht1_temp_), 5, 1, buffer);
        tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
        tft_.setCursor(240 - int(w), 166 + BASELINE_MIDDLE);
        tft_.print(buffer);
      }
      if (abs(double(dht1_hum_ - sensors.getDHT1Hum())) > 0.1) {
        dht1_hum_ = sensors.getDHT1Hum();
        tft_.fillRect(240, 166, 100, HEIGHT_NUMBER_FIELD, colBackColor);
        dtostrf(double(dht1_hum_), 5, 1, buffer);
        strcat_P(buffer, PSTR("%"));
        tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
        tft_.setCursor(340 - int(w), 166 + BASELINE_MIDDLE);
        tft_.print(buffer);
      }
    }
    // DHT 2
    if (sensors.hasDHT2()) {
      if (abs(double(dht2_temp_ - sensors.getDHT2Temp())) > 0.1) {
        dht2_temp_ = sensors.getDHT2Temp();
        tft_.fillRect(160, 192, 80, HEIGHT_NUMBER_FIELD, colBackColor);
        dtostrf(double(dht2_temp_), 5, 1, buffer);
        tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
        tft_.setCursor(240 - int(w), 192 + BASELINE_MIDDLE);
        tft_.print(buffer);
      }
      if (abs(double(dht2_hum_ - sensors.getDHT2Hum())) > 0.1) {
        dht2_hum_ = sensors.getDHT2Hum();
        tft_.fillRect(240, 192, 100, HEIGHT_NUMBER_FIELD, colBackColor);
        dtostrf(double(dht2_hum_), 5, 1, buffer);
        strcat_P(buffer, PSTR("%"));
        tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
        tft_.setCursor(340 - int(w), 192 + BASELINE_MIDDLE);
        tft_.print(buffer);
      }
    }

    // CO2
    if (sensors.hasCO2()) {
      if (abs(mhz14_co2_ppm_ - sensors.getCO2()) > 10) {
        mhz14_co2_ppm_ = sensors.getCO2();
        tft_.fillRect(160, 218, 80, HEIGHT_NUMBER_FIELD, colBackColor);
        snprintf(buffer, sizeof(buffer), "%5d", mhz14_co2_ppm_);
        tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
        tft_.setCursor(240 - int(w), 218 + BASELINE_MIDDLE);
        tft_.print(buffer);
      }
    }

    // VOC
    if (sensors.hasVOC()) {
      if (abs(tgs2600_voc_ppm_ - sensors.getVOC()) > 10) {
        tgs2600_voc_ppm_ = sensors.getVOC();
        tft_.fillRect(160, 244, 80, HEIGHT_NUMBER_FIELD, colBackColor);
        snprintf(buffer, sizeof(buffer), "%5d", tgs2600_voc_ppm_);
        tft_.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
        tft_.setCursor(240 - int(w), 244 + BASELINE_MIDDLE);
        tft_.print(buffer);
      }
    }
  }

private:
  float dht1_temp_ = -1000, dht2_temp_ = -1000;
  float dht1_hum_ = -1000, dht2_hum_ = -1000;
  int mhz14_co2_ppm_ = INT16_MIN / 2, tgs2600_voc_ppm_ = INT16_MIN / 2;
};

/// Setup screen.
class ScreenSetup : public ScreenWithBigTitle
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 6;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetup(TFT& owner) noexcept : ScreenWithBigTitle(owner, F("Einstellungen")) {}

protected:
  virtual void init() noexcept override
  {
    ScreenWithBigTitle::init();

    tft_.setTextColor(colFontColor, colBackColor );

    tft_.setFont(&FreeSans12pt7b);
    tft_.setCursor(18, 121 + BASELINE_MIDDLE);
    tft_.print (F("FAN: Enstellungen Ventilatoren"));
    tft_.setCursor(18, 166 + BASELINE_MIDDLE);
    tft_.print (F("BYP: Einstellungen Sommer-Bypass"));
    tft_.setCursor(18, 211 + BASELINE_MIDDLE);
    tft_.print(F("NET: Netzwerkeinstellungen"));
    tft_.setCursor(18, 256 + BASELINE_MIDDLE);
    tft_.print(F("ZEIT: Zeiteinstellungen"));

    auto& ctrl = getControls<control_type>();

    ctrl.newMenuEntry(1, F("->"),
      [this]() noexcept {
        gotoScreen<ScreenSetup2>();
      }
    );
    ctrl.newMenuEntry(2, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenMain>();
      }
    );
    ctrl.newMenuEntry(3, F("FAN"),
      [this]() noexcept {
        gotoScreen<ScreenSetupFan>();
      }
    );
    ctrl.newMenuEntry(4, F("BYP"),
      [this]() noexcept {
        gotoScreen<ScreenSetupBypass>();
      }
    );
    ctrl.newMenuEntry(5, F("NET"),
      [this]() noexcept {
        gotoScreen<ScreenSetupIPAddress>();
      }
    );
   ctrl. newMenuEntry(6, F("ZEIT"),
      [this]() noexcept {
        gotoScreen<ScreenSetupTime>();
      }
    );
  }
};

/// Setup screen (cont).
class ScreenSetup2 : public ScreenWithBigTitle
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 7;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetup2(TFT& owner) noexcept : ScreenWithBigTitle(owner, F("Einstellungen")) {}

protected:
  virtual void init() noexcept override
  {
    ScreenWithBigTitle::init();

    tft_.setTextColor(colFontColor, colBackColor );

    tft_.setFont(&FreeSans12pt7b);
    tft_.setCursor(18, 121 + BASELINE_MIDDLE);
    tft_.print (F("AF: Enstellungen Frostschutz"));
    tft_.setCursor(18, 166 + BASELINE_MIDDLE);
    tft_.print (F("PGM: Programmeinstellungen"));
  //  tft_.setCursor(18, 211 + BASELINE_MIDDLE);
  //  tft_.print(F("XXX: XXXeinstellungen"));
    tft_.setCursor(18, 256 + BASELINE_MIDDLE);
    tft_.print(F("RST: Werkseinstellungen"));

    auto& ctrl = getControls<control_type>();

    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenSetup>();
      }
    );
  //  newMenuEntry(2, F("XXX"),
  //    [this]() noexcept {
  //      gotoScreen<ScreenSetupXXX>();
  //    }
  //  );
    ctrl.newMenuEntry(3, F("AF"),
      [this]() noexcept {
        gotoScreen<ScreenSetupAntifreeze>();
      }
    );
    ctrl.newMenuEntry(4, F("PGM"),
      [this]() noexcept {
        gotoScreen<ScreenSetupProgram>();
      }
    );
  //  newMenuEntry(5, F("XXX"),
  //    [this]() noexcept {
  //      gotoScreen<ScreenSetupXXX>();
  //    }
  //  );
    ctrl.newMenuEntry(6, F("RST"),
      [this]() noexcept {
        gotoScreen<ScreenSetupFactoryDefaults>();
      }
    );
  }
};

/// Fan setup screen.
class ScreenSetupFan : public ScreenWithInput<ScreenWithBigTitle>
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 8;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetupFan(TFT& owner) noexcept :
    ScreenWithInput(owner, F("Einstellungen Ventilatoren"))
  {
    // copy current state
    auto& config = getControl().getPersistentConfig();
    setpoint_l1_ = config.getSpeedSetpointFan1();
    setpoint_l2_ = config.getSpeedSetpointFan2();
    calculate_speed_mode_ = getControl().getFanControl().getCalculateSpeedMode();
  }

protected:
  virtual void init() noexcept override
  {
    ScreenWithInput::init();

    setupInputFieldColumns(260, 90);
    setupInputFieldRow(1, 1, F("Normdrehzahl Zuluft:"));
    setupInputFieldRow(2, 1, F("Normdrehzahl Abluft:"));
    setupInputFieldColumnWidth(3, 130);
    setupInputFieldRow(3, 1, F("Luefterregelung:"));

    tft_.setFont(&FreeSans9pt7b);
    tft_.setTextColor(colFontColor, colBackColor);
    tft_.setCursor(18, 180 + BASELINE_MIDDLE);
    tft_.print (F("Nach der Aenderung der Normdrehzahlen"));
    tft_.setCursor(18, 200 + BASELINE_MIDDLE);
    tft_.print (F("der Luefter muessen diese kalibriert werden."));
    tft_.setCursor(18, 220 + BASELINE_MIDDLE);
    tft_.print (F("Bei der Kalibrierung werden die Drehzahlen"));
    tft_.setCursor(18, 240 + BASELINE_MIDDLE);
    tft_.print (F("der Luefter eingestellt und die notwendigen"));
    tft_.setCursor(18, 260 + BASELINE_MIDDLE);
    tft_.print (F("PWM-Werte fuer jede Stufe gespeichert."));

    auto& ctrl = getControls<control_type>();

    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenSetup>();
      }
    );
    ctrl.newMenuEntry(2, F("+"),
      [this]() noexcept {
        switch (getCurrentRow()) {
          case 1:
            setpoint_l1_ += 10;
            if (setpoint_l1_ > FanRPM::MAX_RPM)
              setpoint_l1_ = FanRPM::MAX_RPM;
            break;
          case 2:
            setpoint_l2_ += 10;
            if (setpoint_l2_ > FanRPM::MAX_RPM)
              setpoint_l2_ = FanRPM::MAX_RPM;
            break;
          case 3:
            calculate_speed_mode_ = FanCalculateSpeedMode::PID;
            break;
        }
        updateCurrentInputField();
      }
    );
    ctrl.newMenuEntry(3, F("-"),
      [this]() noexcept {
        switch (getCurrentRow()) {
          case 1:
            if (setpoint_l1_ > FanRPM::MIN_RPM + 10)
              setpoint_l1_ -= 10;
            else
              setpoint_l1_ = FanRPM::MIN_RPM;
            break;
          case 2:
            if (setpoint_l2_ > FanRPM::MIN_RPM + 10)
              setpoint_l2_ -= 10;
            else
              setpoint_l2_ = FanRPM::MIN_RPM;
            break;
          case 3:
            calculate_speed_mode_ = FanCalculateSpeedMode::PROP;
            break;
         }
         updateCurrentInputField();
       }
    );
    ctrl.newMenuEntry(4, F("OK"),
      [this]() noexcept {
        resetInput();
        // write to EEPROM and restart
        auto& config = getControl().getPersistentConfig();
        bool speed_changed =
            config.getSpeedSetpointFan1() != setpoint_l1_ ||
            config.getSpeedSetpointFan2() != setpoint_l2_;
        if (speed_changed) {
          config.setSpeedSetpointFan1(setpoint_l1_);
          config.setSpeedSetpointFan2(setpoint_l2_);
          getControl().getFanControl().setCalculateSpeedMode(calculate_speed_mode_);
          doPopup<ScreenSetupFan>(
            F("Einstellungen gespeichert"),
            F("Nenndrehzahlen geaendert.\nBitte Kalibrierung starten."));
        } else if (getControl().getFanControl().getCalculateSpeedMode() != calculate_speed_mode_) {
          getControl().getFanControl().setCalculateSpeedMode(calculate_speed_mode_);
          doPopup<ScreenSetup>(
            F("Einstellungen gespeichert"),
            F("Neuer Modus wurde gespeichert\nund ist sofort aktiv."));
        } else {
          // no change
          gotoScreen<ScreenSetup>();
        }
      }
    );
    ctrl.newMenuEntry(6, F("KAL"),
      [this]() noexcept {
        resetInput();
        auto& config = getControl().getPersistentConfig();
        config.setSpeedSetpointFan1(setpoint_l1_);
        config.setSpeedSetpointFan2(setpoint_l2_);
        auto& fan = getControl().getFanControl();
        fan.setCalculateSpeedMode(calculate_speed_mode_);
        fan.speedCalibrationStart();
        doPopup<ScreenSetup>(
          F("Kalibrierung"),
          F("Luefterkalibrierung wurde gestartet."));
      }
    );
  }

  virtual bool is_changed() noexcept override
  {
    auto& config = getControl().getPersistentConfig();
    return
        config.getSpeedSetpointFan1() != setpoint_l1_ ||
        config.getSpeedSetpointFan2() != setpoint_l2_ ||
        getControl().getFanControl().getCalculateSpeedMode() != calculate_speed_mode_;
  }

private:
  virtual void input_field_draw(uint8_t row, uint8_t col) noexcept override
  {
    char buf[16];
    switch (row) {
      default:
      case 1: snprintf_P(buf, sizeof(buf), PSTR("%u"), setpoint_l1_); break;
      case 2: snprintf_P(buf, sizeof(buf), PSTR("%u"), setpoint_l2_); break;
      case 3: strcpy_P(buf, fanModeToString(calculate_speed_mode_)); break;
    }
    drawCurrentInputField(buf, false);
  }

  /// Mode to calculate fan speed.
  FanCalculateSpeedMode calculate_speed_mode_;
  /// Setpoint for intake fan.
  unsigned setpoint_l1_;
  /// Setpoint for exhaust fan.
  unsigned setpoint_l2_;
};

/// IP configuration setup screen.
class ScreenSetupIPAddress : public ScreenWithInput<ScreenWithBigTitle>
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 9;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetupIPAddress(TFT& owner) noexcept :
    ScreenWithInput(owner, F("Netzwerkeinstellungen"))
  {
    // copy current state
    auto& config = getControl().getPersistentConfig();
    ip_ = config.getNetworkIPAddress();
    mask_ = config.getNetworkSubnetMask();
    gw_ = config.getNetworkGateway();
    mqtt_ = config.getNetworkMQTTBroker();
    mqtt_port_ = config.getNetworkMQTTPort();
    ntp_ = config.getNetworkNTPServer();
    dns_ = config.getNetworkDNSServer();
    mac_ = config.getNetworkMACAddress();
  }

protected:
  virtual void init() noexcept override
  {
    ScreenWithInput::init();

    auto& ctrl = getControls<control_type>();

    setupInputFieldColumns(170, 48);
    setupInputFieldRow(1, 4, F("IP Adresse:"), F("."));
    setupInputFieldRow(2, 4, F("Netzmaske:"), F("."));
    setupInputFieldRow(3, 4, F("Gateway:"), F("."));
    setupInputFieldRow(4, 4, F("MQTT IP:"), F("."));
    setupInputFieldColumnWidth(5, 80);
    setupInputFieldRow(5, 1, F("MQTT port:"));
    setupInputFieldRow(6, 4, F("NTP IP:"), F("."));
    setupInputFieldColumnWidth(7, 32);
    setupInputFieldRow(7, 6, F("MAC"), F(":"));

    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenSetup>();
      }
    );
    ctrl.newMenuEntry(2, F("+10"),
      [this]() noexcept {
        if (getCurrentRow() == 7)
          updateValue(16);
        else
          updateValue(10);
      }
    );
    ctrl.newMenuEntry(3, F("+1"),
      [this]() noexcept {
        updateValue(1);
      }
    );
    ctrl.newMenuEntry(4, F("-1"),
      [this]() noexcept {
        updateValue(-1);
      }
    );
    ctrl.newMenuEntry(5, F("-10"),
      [this]() noexcept {
        if (getCurrentRow() == 7)
          updateValue(-16);
        else
          updateValue(-10);
      }
    );
    ctrl.newMenuEntry(6, F("OK"),
      [this]() noexcept {
        resetInput();
        // write to EEPROM and restart
        auto& config = getControl().getPersistentConfig();
        config.setNetworkIPAddress(ip_);
        config.setNetworkSubnetMask(mask_);
        config.setNetworkGateway(gw_);
        config.setNetworkMQTTBroker(mqtt_);
        config.setNetworkMQTTPort(mqtt_port_);
        config.setNetworkNTPServer(ntp_);
        config.setNetworkDNSServer(dns_);
        doRestart(
          F("Einstellungen gespeichert"),
          F("Die Steuerung wird jetzt neu gestartet."));
      }
    );
  }

private:
  virtual void input_field_enter(uint8_t row, uint8_t col) noexcept override
  {
    IPAddressLiteral* ip;
    switch (getCurrentRow()) {
      default:
      case 1: ip = &ip_; break;
      case 2: ip = &mask_; break;
      case 3: ip = &gw_; break;
      case 4: ip = &mqtt_; break;
      case 5: ip = nullptr; cur_ = mqtt_port_; break;
      case 6: ip = &ntp_; break;
      case 7: ip = nullptr; cur_ = mac_[getCurrentColumn()]; break;
    }
    if (ip)
      cur_ = (*ip)[getCurrentColumn()];
  }

  virtual void input_field_leave(uint8_t row, uint8_t col) noexcept override
  {
    IPAddress old_ip(ip_);
    IPAddress old_mask(mask_);

    IPAddressLiteral* ip;
    switch (getCurrentRow()) {
      default:
      case 1: ip = &ip_; break;
      case 2: ip = &mask_; break;
      case 3: ip = &gw_; break;
      case 4: ip = &mqtt_; break;
      case 5: ip = nullptr; mqtt_port_ = cur_; break;
      case 6: ip = &ntp_; break;
      case 7: ip = nullptr; mac_[getCurrentColumn()] = uint8_t(cur_); break;
    }
    if (ip)
      (*ip)[getCurrentColumn()] = uint8_t(cur_);

    IPAddress new_ip(ip_);
    IPAddress new_mask(mask_);
    IPAddress old_gw(gw_);
    {
      IPAddress gw((new_ip & new_mask) | (old_gw & ~old_mask));
      if (gw != old_gw) {
        updateValueAddress(3, gw_, gw);
        old_gw = gw;
      }
    }
    if (getCurrentRow() <= 3) {
      // gateway potentially changed, check NTP and MQTT
      IPAddress new_gw(gw_);
      if (old_gw == IPAddress(mqtt_))
        updateValueAddress(4, mqtt_, new_gw);
      if (old_gw == IPAddress(ntp_))
        updateValueAddress(6, ntp_, new_gw);
    }
    if (getCurrentRow() <= 2) {
      // netmask or IP changed, check other values
      {
        IPAddress mqtt(mqtt_);
        if ((old_ip & old_mask) == (mqtt & old_mask)) {
          // mqtt server is on the same network, update it
          mqtt = (new_ip & new_mask) | (mqtt & ~old_mask);
          updateValueAddress(4, mqtt_, mqtt);
        }
      }
      {
        IPAddress ntp(ntp_);
        if ((old_ip & old_mask) == (ntp & old_mask)) {
          // NTP server is on the same network, update it
          ntp = (new_ip & new_mask) | (ntp & ~old_mask);
          updateValueAddress(6, ntp_, ntp);
        }
      }
      {
        IPAddress dns(dns_);
        if ((old_ip & old_mask) == (dns & old_mask)) {
          // DNS server is on the same network, update it (not displayed)
          dns_ = IPAddressLiteral((new_ip & new_mask) | (dns & ~old_mask));
        }
      }
    }
  }

  virtual void input_field_draw(uint8_t row, uint8_t col) noexcept override
  {
    char buf[8];
    IPAddressLiteral* ip;
    uint16_t cur;
    switch (row) {
      default:
      case 1: ip = &ip_; break;
      case 2: ip = &mask_; break;
      case 3: ip = &gw_; break;
      case 4: ip = &mqtt_; break;
      case 5: ip = nullptr; cur = mqtt_port_; break;
      case 6: ip = &ntp_; break;
      case 7: ip = nullptr; cur = mac_[col]; break;
    }
    if (ip)
      cur = (*ip)[col];
    if (getCurrentRow() != 7)
      snprintf_P(buf, sizeof(buf), PSTR("%u"), cur);
    else
      snprintf_P(buf, sizeof(buf), PSTR("%02x"), cur);
    drawCurrentInputField(buf);
  }

  void updateValue(int delta) noexcept
  {
    uint16_t new_value;
    if (getCurrentRow() == 2) {
      // netmask change is special, it shifts bits
      if (delta < 0) {
        // shift to left
        new_value = (cur_ << 1) & 255;
      } else {
        new_value = (cur_ >> 1) | 128;
      }
    } else {
      if (delta < 0) {
        delta = -delta;
        if (unsigned(delta) > cur_)
          new_value = 0;
        else
          new_value = cur_ - unsigned(delta);
      } else {
        new_value = cur_ + unsigned(delta);
        if (getCurrentRow() == 5) {
          // MQTT port can have any value
          if (new_value < cur_)
            new_value = 65535;  // overflow
        } else if (new_value > 255) {
          new_value = 255;
        }
      }
    }
    if (new_value == cur_)
      return; // no change

    if (getCurrentRow() == 2) {
      // new netmask validation
      if (new_value != 0) {
        // ensure everything to the left is 255
        auto current_col = getCurrentColumn();
        for (uint8_t col = 0; col < current_col; ++col) {
          if (mask_[col] != 255) {
            mask_[col] = 255;
            updateInputField(getCurrentRow(), col);
          }
        }
      }
      if (new_value != 255) {
        // ensure everything to the right is 0
        auto current_col = getCurrentColumn();
        for (uint8_t col = 3; col > current_col; --col) {
          if (mask_[col] != 0) {
            mask_[col] = 0;
            updateInputField(getCurrentRow(), col);
          }
        }
      }
    }

    cur_ = new_value;
    char buf[8];
    if (getCurrentRow() != 7)
      snprintf_P(buf, sizeof(buf), PSTR("%d"), new_value);
    else
      snprintf_P(buf, sizeof(buf), PSTR("%02x"), new_value);
    drawCurrentInputField(buf);
  }

  void updateValueAddress(uint8_t row, IPAddressLiteral& ip, IPAddress new_ip) noexcept
  {
    // called from leave function to update some addresses
    for (uint8_t i = 0; i < 4; ++i) {
      if (ip[i] != new_ip[i]) {
        ip[i] = new_ip[i];
        updateInputField(row, i);
      }
    }
  }

  /// IP address.
  IPAddressLiteral ip_;
  /// Gateway address.
  IPAddressLiteral gw_;
  /// Network mask.
  IPAddressLiteral mask_;
  /// MQTT server.
  IPAddressLiteral mqtt_;
  /// MQTT port to use.
  uint16_t mqtt_port_;
  /// NTP server.
  IPAddressLiteral ntp_;
  /// DNS server (not displayed).
  IPAddressLiteral dns_;
  /// MAC address.
  MACAddressLiteral mac_;
  /// Value of the current element being modified.
  uint16_t cur_;
};

/// Bypass setup screen.
class ScreenSetupBypass : public ScreenWithInput<ScreenWithBigTitle>
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 10;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetupBypass(TFT& owner) noexcept :
    ScreenWithInput(owner, F("Einstellungen Sommerbypass"))
  {
    // copy current state
    auto& config = getControl().getPersistentConfig();
    temp_outside_min_ = config.getBypassTempAussenluftMin();
    temp_outtake_min_ = config.getBypassTempAbluftMin();
    temp_hysteresis_ = config.getBypassHysteresisTemp();
    min_hysteresis_ = config.getBypassHystereseMinutes();
    if (config.getBypassMode() == SummerBypassMode::USER)
      mode_ = unsigned(config.getBypassManualSetpoint());  // manual (1=closed, 2=open)
    else
      mode_ = unsigned(SummerBypassFlapState::UNKNOWN); // auto
  }

protected:
  virtual void init() noexcept override
  {
    ScreenWithInput::init();

    auto& ctrl = getControls<control_type>();

    setupInputFieldColumns(240, 60);
    setupInputFieldRow(1, 1, F("Temp. Abluft Min:"));
    setupInputFieldRow(2, 1, F("Temp. Aussen Min:"));
    setupInputFieldRow(3, 1, F("Temp. Hysteresis:"));
    setupInputFieldRow(4, 1, F("Hysteresis Minuten:"));
    setupInputFieldColumnWidth(5, 170);
    setupInputFieldRow(5, 1, F("Modus:"));

    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenSetup>();
      }
    );
    ctrl.newMenuEntry(3, F("+"),
      [this]() noexcept {
        updateValue(1);
      }
    );
    ctrl.newMenuEntry(4, F("-"),
      [this]() noexcept {
        updateValue(-1);
      }
    );
    ctrl.newMenuEntry(6, F("OK"),
      [this]() noexcept {
        resetInput();
        auto& config = getControl().getPersistentConfig();
        config.setBypassTempAussenluftMin(temp_outside_min_);
        config.setBypassTempAbluftMin(temp_outtake_min_);
        config.setBypassHysteresisTemp(uint8_t(temp_hysteresis_));
        config.setBypassHystereseMinutes(min_hysteresis_);
        if (mode_ == unsigned(SummerBypassFlapState::UNKNOWN)) {
          config.setBypassMode(SummerBypassMode::AUTO);
        } else {
          config.setBypassMode(SummerBypassMode::USER);
          config.setBypassManualSetpoint(SummerBypassFlapState(mode_));
        }
        doPopup<ScreenSetup>(
          F("Einstellungen gespeichert"),
          F("Neue Bypasseinstellungen wurden\nin EEPROM gespeichert\nund sind sofort aktiv."));
      }
    );
  }

private:
  void updateValue(int8_t delta) noexcept
  {
    int8_t min, max;
    unsigned* cur;
    switch (getCurrentRow()) {
      case 1: min = 5; max = 30; cur = &temp_outtake_min_; break;
      case 2: min = 3; max = 25; cur = &temp_outside_min_; break;
      case 3: min = 1; max = 5;  cur = &temp_hysteresis_; break;
      case 4: min = 5; max = 90; cur = &min_hysteresis_; break;
      case 5: min = 0; max = 2;  cur = &mode_; break;
    }
    delta += *cur;
    if (delta < min)
      delta = min;
    if (delta > max)
      delta = max;
    if (unsigned(delta) == *cur)
      return;
    *cur = unsigned(delta);
    if (getCurrentRow() != 5) {
      char buf[8];
      snprintf_P(buf, sizeof(buf), PSTR("%u"), *cur);
      drawCurrentInputField(buf, false);
    } else {
      drawCurrentInputField(bypassModeToString(SummerBypassFlapState(*cur)), false);
    }
  }

  virtual void input_field_draw(uint8_t row, uint8_t col) noexcept override
  {
    unsigned cur;
    switch (row) {
      default:
      case 1: cur = temp_outtake_min_; break;
      case 2: cur = temp_outside_min_; break;
      case 3: cur = temp_hysteresis_; break;
      case 4: cur = min_hysteresis_; break;
      case 5: cur = mode_; break;
    }
    if (row != 5) {
      char buf[8];
      snprintf_P(buf, sizeof(buf), PSTR("%u"), cur);
      drawCurrentInputField(buf, false);
    } else {
      drawCurrentInputField(bypassModeToString(SummerBypassFlapState(cur)), false);
    }
  }

  /// Minimum inside temperature to open bypass.
  unsigned temp_outtake_min_;
  /// Minimum outside temperature to open bypass.
  unsigned temp_outside_min_;
  /// Bypass temperature hysteresis.
  unsigned temp_hysteresis_;
  /// Bypass hysteresis in minutes.
  unsigned min_hysteresis_;
  /// Bypass mode.
  unsigned mode_;
};

/// Time zone setup screen.
class ScreenSetupTime : public ScreenWithInput<ScreenWithBigTitle>
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 11;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetupTime(TFT& owner) noexcept :
    ScreenWithInput(owner, F("Zeiteinstellungen"))
  {
    // copy current state
    auto& config = getControl().getPersistentConfig();
    timezone_ = config.getTimezoneMin();
    dst_ = config.getDST();
  }

protected:
  virtual void init() noexcept override
  {
    ScreenWithInput::init();

    setupInputFieldColumns(180, 160);
    setupInputFieldColumnWidth(1, 100);
    setupInputFieldRow(1, 1, F("Zeitzone:"));
    setupInputFieldRow(2, 1, F("DST Flag:"));

    auto& ctrl = getControls<control_type>();
    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenSetup>();
      }
    );
    ctrl.newMenuEntry(3, F("+"),
      [this]() noexcept {
        if (getCurrentRow() == 1) {
          // change tz
          if (timezone_ < 24*60)
            timezone_ += 15;
        } else {
           dst_ = !dst_;
        }
        updateCurrentInputField();
      }
    );
    ctrl.newMenuEntry(4, F("-"),
      [this]() noexcept {
        if (getCurrentRow() == 1) {
          // change tz
          if (timezone_ > -24*60)
            timezone_ -= 15;
        } else {
           dst_ = !dst_;
        }
        updateCurrentInputField();
      }
    );
    ctrl.newMenuEntry(6, F("OK"),
      [this]() noexcept {
        resetInput();
        auto& config = getControl().getPersistentConfig();
        config.setTimezoneMin(timezone_);
        config.setDST(dst_);
        doPopup<ScreenSetup>(
          F("Einstellungen gespeichert"),
          F("Neue Zeiteinstellungen wurden\nin EEPROM gespeichert\nund sind sofort aktiv."));
      }
    );
  }

private:
  virtual void input_field_draw(uint8_t row, uint8_t col) noexcept override
  {
    char buf[16];
    switch (row) {
      default:
      case 1:
      {
        auto tz = timezone_;
        char sign = '+';
        if (tz < 0) {
          tz = -tz;
          sign = '-';
        }
        snprintf_P(buf, sizeof(buf), PSTR("%c%02d:%02d"), sign, tz / 60, tz % 60);
        break;
      }
      case 2:
        strcpy_P(buf, dst_ ? PSTR("Sommerzeit") : PSTR("Winterzeit"));
        break;
    }
    drawCurrentInputField(buf, false);
  }

  /// Timezone.
  int16_t timezone_;
  /// Daylight savings time flag.
  bool dst_;
};

/// Antifreeze setup screen.
class ScreenSetupAntifreeze : public ScreenWithInput<ScreenWithBigTitle>
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 12;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetupAntifreeze(TFT& owner) noexcept :
    ScreenWithInput(owner, F("Einstellungen Frostschutz"))
  {
    // copy current state
    auto& config = getControl().getPersistentConfig();
    temp_hysteresis_ = config.getAntifreezeHystereseTemp();
    heating_app_ = config.getHeatingAppCombUse();
  }

protected:
  virtual void init() noexcept override
  {
    ScreenWithInput::init();

    setupInputFieldColumns(300, 80);
    setupInputFieldRow(1, 1, F("Temperaturhysterese:"));
    setupInputFieldRow(2, 1, F("Kaminbetrieb:"));

    auto& ctrl = getControls<control_type>();
    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenSetup2>();
      }
    );
    ctrl.newMenuEntry(3, F("+"),
      [this]() noexcept {
        if (getCurrentRow() == 1) {
          // change tz
          if (temp_hysteresis_ < 10)
            temp_hysteresis_ += 1;
        } else {
           heating_app_ = !heating_app_;
        }
        updateCurrentInputField();
      }
    );
    ctrl.newMenuEntry(4, F("-"),
      [this]() noexcept {
        if (getCurrentRow() == 1) {
          // change tz
          if (temp_hysteresis_ > 1)
            temp_hysteresis_ -= 1;
        } else {
           heating_app_ = !heating_app_;
        }
        updateCurrentInputField();
      }
    );
    ctrl.newMenuEntry(6, F("OK"),
      [this]() noexcept {
        resetInput();
        auto& config = getControl().getPersistentConfig();
        config.setAntifreezeHystereseTemp(temp_hysteresis_);
        config.setHeatingAppCombUse(heating_app_);
        getControl().getAntifreeze().begin(Serial);  // restart antifreeze
        doPopup<ScreenSetup2>(
          F("Einstellungen gespeichert"),
          F("Einstellungen der Frostschutzschaltung\nwurden in EEPROM gespeichert\nund sind sofort aktiv."));
      }
    );
  }

private:
  virtual void input_field_draw(uint8_t row, uint8_t col) noexcept override
  {
    char buf[8];
    switch (row) {
      default:
      case 1:
      {
        snprintf_P(buf, sizeof(buf), PSTR("%d"), temp_hysteresis_);
        break;
      }
      case 2:
        strcpy_P(buf, heating_app_ ? PSTR("JA") : PSTR("NEIN"));
        break;
    }
    drawCurrentInputField(buf, false);
  }

  /// Antifreeze temperature hysteresis.
  unsigned temp_hysteresis_;
  /// Flag whether using heating application together with ventilation system.
  bool heating_app_;
};

/// Program setup screen.
class ScreenSetupProgram : public ScreenWithInput<ScreenWithBigTitle>
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 13;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetupProgram(TFT& owner) noexcept :
    ScreenWithInput(owner, F("Programmmanager"))
  {
    index_ = -1;
    memset(&pgm_, 0, sizeof(pgm_));
  }

protected:
  virtual void init() noexcept override
  {
    ScreenWithInput::init();

    setupInputFieldColumns(210, 40);
    setupInputFieldRow(1, 1, F("Programm:"));
    setupInputFieldRow(2, 1, F("Stufe:"));
    setupInputFieldRow(3, 2, F("Startzeit:"), F(":"));
    setupInputFieldRow(4, 2, F("Endzeit:"), F(":"));
    setupInputFieldColumnWidth(5, 200);
    setupInputFieldRow(5, 1, F("Wochentage:"));
    setupInputFieldColumnWidth(6, 120);
    setupInputFieldRow(6, 1, F("Programmsaetze:"));

    auto& ctrl = getControls<control_type>();
    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        updateProgram(0);
      }
    );
    ctrl.newMenuEntry(3, F("+"),
      [this]() noexcept {
        updateProgram(1);
      }
    );
    ctrl.newMenuEntry(4, F("-"),
      [this]() noexcept {
        updateProgram(-1);
      }
    );
    ctrl.newMenuEntry(5, F("OK"),
      [this]() noexcept {
        // store changes
        resetInput();
        auto& config = getControl().getPersistentConfig();
        if (index_ >= 0) {
          config.setProgram(uint8_t(index_), pgm_);
          doPopup<ScreenSetupProgram>(
            F("Einstellungen gespeichert"),
            F("Neue Programmeinstellungen\nwurden in EEPROM gespeichert\nund sind sofort aktiv."));
        } else {
          doPopup<ScreenSetupProgram>(
            F("Keine Programmnummer"),
            F("Bitte zuerst Programmnummer waehlen."));
        }
      }
    );
    ctrl.newMenuEntry(6, F("RST"),
      [this]() noexcept {
        // reload program, cancel changes
        resetInput();
        auto& config = getControl().getPersistentConfig();
        if (index_ >= 0) {
          pgm_ = config.getProgram(uint8_t(index_));
          doPopup<ScreenSetupProgram>(
            F("Einstellungen zurueckgesetzt"),
            F("Die Programmeinstellungen\nwurden zurueckgesetzt."));
        }
      }
    );
  }

private:
  void updateProgram(int8_t delta)
  {
    auto& config = getControl().getPersistentConfig();
    if (getCurrentRow() == 1 || delta == 0) {
      // index change, check whether program data changed
      if (index_ >= 0 && pgm_ != config.getProgram(uint8_t(index_))) {
        // program changed, but not saved
        doPopup<ScreenSetupProgram>(
          F("Einstellungen nicht gespeichert"),
          F("Die geaenderte Programmeinstellungen\nbitte zuerst speichern mit OK oder\nzuruecksetzen mit RST."));
        return;
      }
      // data unchanged
      if (delta == 0) {
        // special case for "<-" button
        gotoScreen<ScreenSetup2>();
        return;
      }
      index_ += delta;
      if (index_ < 0) {
        index_ = -1;
        memset(&pgm_, 0, sizeof(pgm_));
      } else {
        if (index_ >= KWLConfig::MaxProgramCount)
          index_ = KWLConfig::MaxProgramCount - 1;
        pgm_ = config.getProgram(uint8_t(index_));
      }
      // refresh all program display fields, since new program was loaded in pgm_
      updateAllInputFields();
      updateCurrentInputField();
      return;
    }
    if (index_ == -1) {
      doPopup<ScreenSetupProgram>(
        F("Keine Programmnummer"),
        F("Bitte zuerst Programmnummer waehlen."));
      return; // no update, if no program selected
    }

    // normal field update
    const bool is_min = (getCurrentColumn() == 1);
    const uint8_t max = is_min ? 59 : 23;
    switch (getCurrentRow()) {
      case 2:
        update_minmax<uint8_t>(pgm_.fan_mode_, delta, 0, KWLConfig::StandardModeCnt - 1);
        break;
      case 3:
        update_minmax<uint8_t>(is_min ? pgm_.start_m_ : pgm_.start_h_, delta, 0, max);
        break;
      case 4:
        update_minmax<uint8_t>(is_min ? pgm_.end_m_ : pgm_.end_h_, delta, 0, max);
        break;
      case 5:
        // popup with weekday flags
        doPopup<ScreenSetupProgram>(F("Einstellungen Wochentage"), F("Wochentage, an denen\ndas Programm laufen soll:"));
        // setup popup flags
        popup_flags_.flags_ = &pgm_.weekdays_;
        popup_flags_.flag_name_length_ = 2;
        popup_flags_.flag_count_ = 7;
        popup_flags_.flag_names_ = F("MoDiMiDoFrSaSo");
        setPopupFlags(popup_flags_);
        return;
      case 6:
        // popup with set flags
        doPopup<ScreenSetupProgram>(F("Einstellungen Programmsatz"), F("Programmsaetze, in denen\ndas Programm laufen soll:"));
        // setup popup flags
        popup_flags_.flags_ = &pgm_.enabled_progsets_;
        popup_flags_.flag_name_length_ = 1;
        popup_flags_.flag_count_ = 8;
        popup_flags_.flag_names_ = F("01234567");
        setPopupFlags(popup_flags_);
        return;
    }
    updateCurrentInputField();
  }

  virtual void input_field_draw(uint8_t row, uint8_t col) noexcept override
  {
    if (index_ < 0) {
      // empty screen
      drawCurrentInputField("", false);
      return;
    }
    char buf[16];
    switch (row) {
      default:
      case 1:
        snprintf_P(buf, sizeof(buf), PSTR("%02d"), index_);
        break;
      case 2:
        snprintf_P(buf, sizeof(buf), PSTR("%d"), pgm_.fan_mode_);
        break;
      case 3:
        snprintf_P(buf, sizeof(buf), PSTR("%02d"), (col == 0) ? pgm_.start_h_ : pgm_.start_m_);
        break;
      case 4:
        snprintf_P(buf, sizeof(buf), PSTR("%02d"), (col == 0) ? pgm_.end_h_ : pgm_.end_m_);
        break;
      case 5:
        format_flags(buf, F("MoDiMiDoFrSaSo"), 7, 2, pgm_.weekdays_);
        break;
      case 6:
        format_flags(buf, F("01234567"), 8, 1, pgm_.enabled_progsets_);
        break;
    }
    drawCurrentInputField(buf, false);
  }

  /// Current program index to edit.
  int8_t index_;
  /// Program data of the current program.
  ProgramData pgm_;
  /// Currently edited popup flags.
  PopupFlagsState popup_flags_;
};

/// Factory defaults screen.
class ScreenSetupFactoryDefaults : public ScreenWithBigTitle
{
public:
  /// Screen ID.
  static constexpr uint32_t ID = 1 << 14;
  /// Screen IDs of this screen and all base classes.
  static constexpr uint32_t IDS = ID | ScreenWithBigTitle::IDS;

  using control_type = ControlMenuButtons;

  explicit ScreenSetupFactoryDefaults(TFT& owner) noexcept :
    ScreenWithBigTitle(owner, F("Werkseinstellungen"))
  {
  }

protected:
  virtual void init() noexcept override
  {
    tft_.setTextColor(colFontColor, colBackColor);
    tft_.setFont(&FreeSans9pt7b);

    tft_.setCursor(18, 125 + BASELINE_MIDDLE);
    tft_.print (F("Es werden alle Werte der Steuerung auf die"));
    tft_.setCursor(18, 150 + BASELINE_MIDDLE);
    tft_.print (F("Standardwerte zurueckgesetzt."));
    tft_.setCursor(18, 175 + BASELINE_MIDDLE);
    tft_.print(F("Die Steuerung wird anschliessend neu gestartet."));

    auto& ctrl = getControls<control_type>();
    ctrl.newMenuEntry(1, F("<-"),
      [this]() noexcept {
        gotoScreen<ScreenSetup2>();
      }
    );
    ctrl.newMenuEntry(6, F("OK"),
      [this]() noexcept {
        Serial.print(F("Speicherbereich wird geloescht... "));
        tft_.setFont(&FreeSans9pt7b);
        tft_.setTextColor(colFontColor, colBackColor);
        tft_.setCursor(18, 220 + BASELINE_MIDDLE);
        tft_.print(F("Speicherbereich wird geloescht... "));

        getControl().getPersistentConfig().factoryReset();
        tft_.println(F("OK"));
        Serial.println(F("OK"));

        doRestart(
          F("Einstellungen gespeichert"),
          F("Werkseinstellungen wiederhergestellt.\nDie Steuerung wird jetzt neu gestartet."));
      }
    );
  }
};

void TFT::Screen::update() noexcept
{
  auto last_touch = getLastTouchTime();
  if (last_touch) {
    auto time = millis() - last_touch;
    if (time > INTERVAL_DISPLAY_TIMEOUT) {
      // turn off display
      if (KWLConfig::serialDebugDisplay)
        Serial.println(F("TFT: Display timed out, turning it off"));
      gotoScreen<ScreenSaver>();
      return;
    } else if (time > INTERVAL_TOUCH_TIMEOUT) {
      // go to main screen if too long not touched
      if (owner_.current_screen_id_ != ScreenMain::ID) {
        // current screen is not the main screen
        if (KWLConfig::serialDebugDisplay) {
          Serial.print(F("TFT: Touch timeout, go to main screen, previous="));
          Serial.println(owner_.current_screen_id_);
        }
        gotoScreen<ScreenMain>();
      }
    }
  }
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
  current_screen_->update();  // update initial screen to get timestamp for wait start
}

template<typename ScreenClass>
inline void TFT::gotoScreen() noexcept
{
  if (current_screen_id_ == ScreenClass::ID) {
    // we are already on the right screen
    current_control_->init();
    current_screen_->init();
    displayUpdate();
    return;
  }

  using ControlClass = typename ScreenClass::control_type;
  static_assert(sizeof(ScreenClass) + sizeof(ControlClass) <= sizeof(dynamic_space_), "Screen+control too big");

  // instantiate new screen
  current_control_ = new(dynamic_space_ + sizeof(ScreenClass)) ControlClass(*this);
  current_screen_ = new(dynamic_space_) ScreenClass(*this);

  current_control_->init();
  current_screen_->init();

  last_screen_ids_ = ScreenClass::IDS;
  current_screen_id_ = ScreenClass::ID;

  displayUpdate();
}

void TFT::displayUpdate() noexcept
{
  if (KWLConfig::serialDebugDisplay == 1)
    Serial.println(F("TFT: displayUpdate"));

  // Das Update wird alle 1000mS durchlaufen
  // Bevor Werte ausgegeben werden, wird auf Änderungen der Werte überprüft, nur geänderte Werte werden auf das Display geschrieben
  if (current_screen_)
    current_screen_->update();

  display_update_task_.runRepeated(INTERVAL_DISPLAY_UPDATE);
}

TSPoint TFT::getPoint() noexcept
{
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
  return tp;
}

void TFT::loopTouch() noexcept
{
  TSPoint tp = getPoint();
  auto time = millis();
  if (!time)
    time = 1;

  if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
    // pressed
    int16_t xpos, ypos;  //screen coordinates

    // is controller wired for Landscape ? or are we oriented in Landscape?
    if (KWLConfig::TouchSwapXY != (KWLConfig::TFTOrientation & 1))
      SWAP(tp.x, tp.y);

    // scale from 0->1023 to tft_.width  i.e. left = 0, rt = width
    // most mcufriend have touch (with icons) that extends below the TFT
    // screens without icons need to reserve a space for "erase"
    // scale the ADC values from ts.getPoint() to screen values e.g. 0-239
    xpos = int(map(tp.x, TS_LEFT, TS_RT, 0, tft_.width()));
    ypos = int(map(tp.y, TS_TOP, TS_BOT, 0, tft_.height()));

    if (KWLConfig::serialDebugDisplay) {
      Serial.print(F("Touch (xpos/ypos, tp.x/tp.y): "));
      Serial.print(xpos);
      Serial.print('/');
      Serial.print(ypos);
      Serial.print(',');
      Serial.print(tp.x);
      Serial.print('/');
      Serial.print(tp.y);
      Serial.print(F(", ms="));
      Serial.println(time);
    }

    touch_in_progress_ = true;
    millis_last_touch_ = time;
    bool consumed = false;
    if (current_screen_)
      consumed = current_screen_->touch(xpos, ypos, time);
    if (current_control_ && !consumed)
      consumed = current_control_->touch(xpos, ypos, time);

  } else if (touch_in_progress_) {
    // released
    if (KWLConfig::serialDebugDisplay) {
      Serial.print(F("Touch release: ms="));
      Serial.println(time);
    }
    touch_in_progress_ = false;
    if (current_control_)
      current_control_->release(time);
    if (current_screen_)
      current_screen_->release(time);
  }
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

  gotoScreen<ScreenInit>();
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
