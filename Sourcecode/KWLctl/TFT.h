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
  class Screen;
  class ScreenCalibration;
  class Control;

  TFT() noexcept;

  /// Perform minimal setup of TFT screen before starting the boot process.
  void setup() noexcept
  {
    setupDisplay();
    setupTouch();
  }

  /// Get printer for printing boot messages on TFT. Only valid after call to setup().
  Print& getTFTPrinter() noexcept { return tft_; }

  /// Get display handler. Only valid after call to setup().
  MCUFRIEND_kbv& getTFT() noexcept { return tft_; }

  /// Start TFT component.
  void begin(Print& initTracer, KWLControl& control) noexcept;

  /// Prepare for screenshot by waking from display off.
  void prepareForScreenshot() noexcept;

  /// Switch to given screen.
  void gotoScreen(int id) noexcept;

  /// Simulate touch at given coordinates.
  void makeTouch(int x, int y) noexcept;

private:
  friend class Screen;
  friend class ScreenCalibration;
  friend class Control;

  /*!
   * @brief Check whether last screen displayed was of the specified class.
   *
   * This can be used, e.g., to skip drawing common parts of the screen.
   */
  template<typename ScreenClass>
  inline bool lastScreenWas() noexcept
  {
    return (last_screen_ids_ & ScreenClass::ID) != 0;
  }

  /*!
   * @brief Change to the specified screen class.
   *
   * @tparam ScreenClass screen class to display.
   */
  template<typename ScreenClass>
  inline void gotoScreen() noexcept;

  /// Get main controller object.
  KWLControl& getControl() const noexcept { return *control_; }

  /// Display update function to be called periodically.
  void displayUpdate() noexcept;

  /// Safely get touch point.
  TSPoint getPoint() noexcept;

  /// Poll for touch events.
  void loopTouch() noexcept;

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

  /// Currently-displayed screen.
  Screen* current_screen_ = nullptr;

  /// Current screen ID.
  uint32_t current_screen_id_ = 0;

  /// Type hierarchy of last screen.
  uint32_t last_screen_ids_ = 0;

  /// Calibration for touch.
  const TouchCalibration* cal_;

  /// Set if a touch is currently detected.
  bool touch_in_progress_ = false;

  /// Last time a touch input was registered.
  unsigned long millis_last_touch_ = 0;

  /// Space for screen and controls.
  char dynamic_space_[150];

  /// Statistics for display update.
  Scheduler::TaskTimingStats display_update_stats_;
  /// Task to update display.
  Scheduler::TimedTask<TFT> display_update_task_;
  /// Statistics for touch input.
  Scheduler::TaskPollingStats process_touch_stats_;
  /// Task to process touch input.
  Scheduler::PollTask<TFT> process_touch_task_;
};
