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

  /// Set up screen for normal speed of fan 1.
  void screenSetupFan1() noexcept;

  /// Set up screen for normal speed of fan 2.
  void screenSetupFan2() noexcept;

  /// Set up screen for normal speed of fan 1 or 2.
  void screenSetupFan(uint8_t fan_idx) noexcept;

  /// Update fan speed on the screen.
  void displayUpdateFan() noexcept;

  /// Set up screen for calibrating fans.
  void screenSetupFanCalibration() noexcept;

  /// Set up screen for reset to factory defaults.
  void screenSetupFactoryDefaults() noexcept;

  /// Set up screen for selecting fan calibration mode.
  void screenSetupFanMode() noexcept;

  /// Update fan calibration mode on the screen.
  void displayUpdateFanMode() noexcept;

  /// Display update function to called periodically.
  void displayUpdate() noexcept;

  /*!
   * @brief Change to a different screen.
   *
   * @param screenSetup setup routine for the screen.
   */
  void gotoScreen(void (TFT::*screenSetup)()) noexcept;

  /// Clear background behind title line and print new title line.
  void printScreenTitle(char* title) noexcept;

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

  /// Print a menu entry with given color for the frame.
  void printMenuBtn(byte mnuBtn, const __FlashStringHelper* mnuTxt, long colFrame) noexcept;

  /// Draw menu border around given menu button.
  void setMenuBorder(byte menuBtn) noexcept;

  /// Print header in the top line of the display.
  void printHeader() noexcept;

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

  /// Per-screen state values, e.g., last display values or input values.
  union {
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
      /// Input value for the new setpoint.
      unsigned input_standard_speed_setpoint_;
      /// Fan index for which we are setting the default speed.
      uint8_t fan_index_;
    } fan_;

    /// Fan speed calculation mode setup.
    FanCalculateSpeedMode fan_calculate_speed_mode_;
  } screen_state_;

  /// Statistics for display update.
  Scheduler::TaskTimingStats display_update_stats_;
  /// Task to update display.
  Scheduler::TimedTask<TFT> display_update_task_;
  /// Statistics for touch input.
  Scheduler::TaskPollingStats process_touch_stats_;
  /// Task to process touch input.
  Scheduler::PollTask<TFT> process_touch_task_;
};
