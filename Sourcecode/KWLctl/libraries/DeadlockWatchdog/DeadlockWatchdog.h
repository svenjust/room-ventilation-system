/*
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

/*!
 * @file
 * @brief Watchdog which resets the system on deadlock.
 */
#pragma once

/*!
 * @brief Watchdog which resets the system on deadlock.
 *
 * When the system deadlocks and doesn't respond for some time, the watchdog
 * timer fires. It calls a custom routine, which can for instance store crash
 * data into EEPROM, so it's available at the restart.
 *
 * The implementation is inspired by https://github.com/Megunolink/ArduinoCrashMonitor.
 */
class DeadlockWatchdog
{
public:
  /*!
   * @brief Reporting function to call at crash time.
   *
   * @param pc program counter at the time of the watchdog hit.
   * @param sp stack pointer at the time of the watchdog hit.
   * @param arg user-specified argument.
   */
  using report_fnc = void(*)(unsigned long pc, unsigned sp, void* arg);

  /*!
   * @brief Start watching for deadlock.
   *
   * The watchdog is set to maximum time (8s). I.e., if no reset() call is
   * made within the time period, the watchdog will hit.
   *
   * @param f function to report an error to.
   * @param f_arg argument to pass to the function.
   */
  static void begin(report_fnc f, void* f_arg = nullptr) noexcept;

  /*!
   * @brief Start watching for deadlock.
   *
   * The watchdog is set to the timeout specified by the user. If no reset()
   * call is made within the time period, the watchdog will hit.
   *
   * @param f function to report an error to.
   * @param to timeout (one of WDTO_* constants).
   * @param f_arg argument to pass to the function.
   */
  static void begin(report_fnc f, unsigned char to, void* f_arg = nullptr) noexcept;

  /*!
   * @brief Reset the watchdog.
   *
   * Reset the timer, so the time period begins anew.
   *
   * Normally, you would call this in loop().
   */
  static void reset() noexcept;

  /*!
   * @brief Disable the watchdog.
   *
   * It can be reenabled by a new call to begin().
   */
  static void disable() noexcept;
};


