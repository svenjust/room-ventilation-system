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
 * @brief Fan speed measurement library.
 */

#pragma once

/*!
 * @brief Stores measurements and computes fan speed.
 *
 * This class measures rotations per minute of a fan reporting its speed
 * by sending a signal once per rotation. The signal is captured from a pin
 * using an associated interrupt. Interrupt's handling routine must call
 * interrupt() routine.
 *
 * To actually report the measurement, it must be first captured from the
 * volatile internal state using capture() call under interrupts disabled.
 * The routine doesn't disable interrupts by itself, so you can call it
 * for several fans. Afterwards, you can read the speed using get_speed()
 * routine.
 *
 * You can dump the internal state to serial console using dump() method,
 * but this method is not synchronized (i.e., it may report erratic data).
 */
class FanRPM
{
public:
  /// Initialize the fan speed measurement.
  FanRPM();

  /// Call this method in the interrupt function for the RPM measurement pin.
  void interrupt();

  /*!
   * @brief Capture the current fan speed.
   *
   * @note This method must be called under interrupts disabled.
   *
   * After capture finishes, you can use get_speed() to get the actual speed.
   */
  inline void capture()
  {
    captured_valid_ = valid_;
    captured_sum_ = sum_;
  }

  /*!
   * @brief Get the last captured measured speed in rpm.
   *
   * Before calling this method, call capture() under interrupts disabled
   * to transfer volatile state.
   */
  unsigned long get_speed();

  /// Dump the internal state to the serial console (unsynchronized read).
  void dump();

private:
  enum {
    /// Maximum # of measurements to store. Must be power of 2.
    MAX_MEASUREMENTS = 1<<5,
    /// Minimum valid RPM.
    MIN_RPM = 60,
    /// Maximum valid RPM.
    MAX_RPM = 10000
  };

  /// Last measurement time.
  unsigned long last_time_ = 0;
  /// Measurements buffer.
  unsigned long measurements_[MAX_MEASUREMENTS] = {0, };
  /// Current measurement index. Wraps around the buffer.
  unsigned char index_ = 0;
  /// Set to true, if a valid measurement was found.
  volatile bool valid_ = false;
  /// Validity flag captured by capture() call.
  bool captured_valid_ = false;
  /// Last measurement. Used to filter out outliers.
  unsigned long last_ = 0;
  /// Current sum of all measurements.
  volatile unsigned long sum_ = 0;
  /// Measurement captured by capture() call.
  unsigned long captured_sum_ = 0;
};
