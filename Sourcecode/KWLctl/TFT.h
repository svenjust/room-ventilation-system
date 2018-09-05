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
#pragma once

#include <TimeScheduler.h>
#include <MCUFRIEND_kbv.h>      // TFT
#include <TouchScreen.h>        // Touch

#include "FanControl.h"

class KWLControl;

/*!
 * @brief TFT and touchscreen controller.
 *
 * This class handles TFT display and touch input.
 */
class TFT
{
public:
  TFT() noexcept;

  /// Perform minimal setup of TFT screen before starting the boot process.
  void setup() noexcept
  {
    setupDisplay();
    setupTouch();
  }

  /// Get printer for printing boot messages on TFT. Only valid after call to setup().
  Print& getTFTPrinter() noexcept { return tft_; }

  /// Start TFT component.
  void begin(Print& initTracer, KWLControl& control) noexcept;

private:

  /// Menu action for a button.
  class MenuAction
  {
  public:
    MenuAction() = default;

    /// Store function object into the menu action.
    template<typename Func>
    MenuAction& operator=(Func&& f) noexcept;

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

  /// Set up main screen.
  void screenMain() noexcept;

  /// Update main screen as necessary.
  void displayUpdateMain() noexcept;

  /// Set up screen for additional sensors.
  void screenAdditionalSensors() noexcept;

  /// Update additional sensors screen as necessary.
  void displayUpdateAdditionalSensors() noexcept;

  /// Perform menu action on additional sensors screen.
  void menuActionAdditionalSensors(uint8_t button) noexcept;

  /// Set up screen for setting up various parameters.
  void screenSetup() noexcept;

  /// Set up screen for setting up various fan parameters.
  void screenSetupFan() noexcept;

  /// Set up screen for reset to factory defaults.
  void screenSetupFactoryDefaults() noexcept;

  /// Set up screen for selecting fan calibration mode.
  void screenSetupFanMode() noexcept;

  /// Update fan calibration mode on the screen.
  void displayUpdateFanMode() noexcept;

  /// Set up IP addresses.
  void screenSetupIPAddress() noexcept;

  /// Update current octet of IP address by given delta.
  void screenSetupIPAddressUpdate(int delta) noexcept;

  /// Update IP address in memory and in given row.
  void screenSetupIPAddressUpdateAddress(uint8_t row, IPAddressLiteral& ip, IPAddress new_ip) noexcept;

  /// Set up bypass.
  void screenSetupBypass() noexcept;

  /// Update current value by given delta.
  void screenSetupBypassUpdate(int delta) noexcept;

  /// Set up timezone.
  void screenSetupTime() noexcept;

  /// Display update function to be called periodically.
  void displayUpdate() noexcept;

  /*!
   * @brief Change to a different screen.
   *
   * @param screenSetup setup routine for the screen.
   */
  void gotoScreen(void (TFT::*screenSetup)()) noexcept;

  /// Clear background behind title line and print new title line.
  void printScreenTitle(const __FlashStringHelper* title) noexcept;

  /// Poll for touch events.
  void loopTouch() noexcept;

  /*!
   * @brief Set up new menu entry when creating a screen.
   *
   * @param mnuBtn button index.
   * @param mnuTxt button text.
   * @param action action to execute when button pressed.
   */
  template<typename Func>
  void newMenuEntry(byte mnuBtn, const __FlashStringHelper* mnuTxt, Func&& action) noexcept;

  /*!
   * @brief Set up input action when the user selected an input field.
   *
   * Actions are called when input_current_* variables are properly set up.
   * Typically, the action will call drawCurrentInputField().
   *
   * @param draw action to call to draw a field (current row/column and highlight is set).
   */
  template<typename Func>
  void setupInputAction(Func&& draw) noexcept;

  /*!
   * @brief Set up input action when the user selected an input field.
   *
   * Actions are called when input_current_* variables are properly set up.
   * Typically, the action will call drawCurrentInputField().
   *
   * @param start_input action to call before entering an input field (drawn with highlight after).
   * @param stop_input action to call after leaving an input field (redraw w/o highlight before).
   * @param draw action to call to draw a field (current row/column and highlight is set).
   */
  template<typename Func1, typename Func2, typename Func3>
  void setupInputAction(Func1&& start_input, Func2&& stop_input, Func3&& draw) noexcept;

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
  void setupInputFieldColumns(int left = 180, int width = 50, int spacing = 10) noexcept;

  /*!
   * @brief Set up input field row.
   *
   * @param row input field row.
   * @param count count of input fields.
   * @param header input field header to print.
   * @param separator if set, this will be printed in spaces between input columns (e.g., '.' for IP address).
   */
  void setupInputFieldRow(uint8_t row, uint8_t count, const char* header, const __FlashStringHelper* separator = nullptr) noexcept;

  /*!
   * @brief Set up input field row.
   *
   * @param row input field row.
   * @param count count of input fields.
   * @param header input field header to print.
   * @param separator if set, this will be printed in spaces between input columns (e.g., '.' for IP address).
   */
  void setupInputFieldRow(uint8_t row, uint8_t count, const __FlashStringHelper* header, const __FlashStringHelper* separator = nullptr) noexcept;

  /*!
   * @brief Reset active input field, if any.
   */
  void resetInput() noexcept;

  /*!
   * @brief Draw the contents of an input field, e.g., upon screen setup.
   *
   * @param row,col row and column of the input field.
   * @param text field value.
   * @param highlight set to @c true to draw the field highlighted.
   * @param right_align if set to @c true, align on the right side, else on the left side.
   */
  void drawInputField(uint8_t row, uint8_t col, const char* text, bool highlight = false, bool right_align = true) noexcept;

  /*!
   * @brief Draw the contents of an input field, e.g., upon screen setup.
   *
   * @param row,col row and column of the input field.
   * @param text field value.
   * @param highlight set to @c true to draw the field highlighted.
   * @param right_align if set to @c true, align on the right side, else on the left side.
   */
  void drawInputField(uint8_t row, uint8_t col, const __FlashStringHelper* text, bool highlight = false, bool right_align = true) noexcept;

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

  /// Print a menu entry with given color for the frame.
  void printMenuBtn(byte mnuBtn, const __FlashStringHelper* mnuTxt, uint16_t colFrame) noexcept;

  /// Draw menu border around given menu button.
  void setMenuBorder(byte menuBtn) noexcept;

  /// Print header in the top line of the display.
  void printHeader() noexcept;

  /*!
   * @brief Restart the controller after displaying the message.
   *
   * @param title popup window title.
   * @param message message to show in the window.
   */
  void doRestart(const __FlashStringHelper* title, const __FlashStringHelper* message) noexcept;

  /*!
   * @brief Show popup and go to specified screen afterwards.
   *
   * @param title popup window title.
   * @param message message to show in the window.
   * @param screenSetup setup routine for the screen.
   */
  void doPopup(const __FlashStringHelper* title, const __FlashStringHelper* message, void (TFT::*screenSetup)()) noexcept;

  /// Restart the controller immediately.
  void doRestartImpl() noexcept;

  /// Set up display before first use.
  void setupDisplay() noexcept;

  /// Set up touch input.
  void setupTouch();

  /// TFT object.
  MCUFRIEND_kbv tft_;

  /// Touchscreen object.
  TouchScreen ts_;

  /// Control instance.
  KWLControl* control_ = nullptr;

  /// Bound member function to update display for the current screen (called every second).
  void (TFT::*display_update_)() = nullptr;

  /// Time at which startup delay started or 0 for normal operation.
  unsigned long millis_startup_delay_start_ = 0;

  // Touch and menu handling:

  /// Maximum menu button count.
  static constexpr unsigned MENU_BTN_COUNT = 6;
  /// Last menu button, which was highlighted.
  byte last_highlighed_menu_btn_ = 0;
  /// Time at which the menu button was last pressed.
  unsigned long millis_last_menu_btn_press_ = 0;
  /// Time at which the display was touched.
  unsigned long millis_last_touch_ = 0;
  /// Menu actions.
  MenuAction menu_btn_action_[MENU_BTN_COUNT];

  /// Maximum # of input fields per row. Y positions and height is given by menu buttons.
  static constexpr unsigned INPUT_COL_COUNT = 4;
  /// Maximum # of input field rows.
  static constexpr unsigned INPUT_ROW_COUNT = 7;
  /// Input field X coordinates.
  int input_x_[INPUT_COL_COUNT];
  /// Input field width.
  int input_w_[INPUT_COL_COUNT];
  /// Bitmask of active input fields for individual menu rows.
  uint8_t input_active_[INPUT_ROW_COUNT];
  /// Currently selected input row (1-based, like menus).
  uint8_t input_current_row_;
  /// Currently selected input column (0-based).
  uint8_t input_current_col_;
  /// Highlight to use when drawing a field.
  bool input_highlight_ = false;
  /// Action to call when the user selected an input field. Fields input_current_* already updated.
  MenuAction input_field_enter_;
  /// Action to call when the user deselected an input field. Fields input_current_* point to left field.
  MenuAction input_field_leave_;
  /// Action to call to draw the initial state of an input field. Fields input_current_* point to left field.
  MenuAction input_field_draw_;

  // Global screen state values:

  /// Last shown error bits.
  unsigned last_error_bits_ = 0;
  /// Last shown info bits.
  unsigned last_info_bits_  = 0;
  /// Last shown network state.
  bool last_lan_ok_  = true;
  /// Last shown MQTT state.
  bool last_mqtt_ok_ = true;
  /// Force display update also of not changed values.
  bool force_display_update_ = false;
  /// Time at which the popup started showing.
  unsigned long millis_popup_show_time_ = 0;
  /// Action for popup timeout and OK button.
  void (TFT::*popup_action_)() = nullptr;

  /// Per-screen state values, e.g., last display values or input values.
  union screen_state
  {
    screen_state() {}
    ~screen_state() {}

    /// Last display values for main screen.
    struct
    {
      int tacho_fan1_;
      int tacho_fan2_;
      int kwl_mode_;
      int efficiency_;
      double t1_, t2_, t3_, t4_;
    } main_;

    /// Last display values for additional sensors.
    struct
    {
      float dht1_temp_, dht2_temp_;
      int mhz14_co2_ppm_, tgs2600_voc_ppm_;
    } addt_sensors_;

    /// Fan setup.
    struct
    {
      /// Mode to calculate fan speed.
      FanCalculateSpeedMode calculate_speed_mode_;
      /// Setpoint for intake fan.
      unsigned setpoint_l1_;
      /// Setpoint for exhaust fan.
      unsigned setpoint_l2_;
    } fan_;

    /// Network setup.
    struct
    {
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
    } net_;

    /// Bypass setup.
    struct {
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
    } bypass_;

    /// Time setup.
    struct {
      /// Timezone.
      int16_t timezone_;
      /// Daylight savings time flag.
      bool dst_;
    } time_;
  };

  /// State for individual screens.
  screen_state screen_state_;

  /// Statistics for display update.
  Scheduler::TaskTimingStats display_update_stats_;
  /// Task to update display.
  Scheduler::TimedTask<TFT> display_update_task_;
  /// Statistics for touch input.
  Scheduler::TaskPollingStats process_touch_stats_;
  /// Task to process touch input.
  Scheduler::PollTask<TFT> process_touch_task_;
};
