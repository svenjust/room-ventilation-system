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

class Print;

/*!
 * @brief Stores measurements and computes fan speed.
 *
 * This class measures rotations per minute of a fan reporting its speed
 * by sending a signal once per rotation. The signal is captured from a pin
 * using an associated interrupt. Interrupt's handling routine must call
 * interrupt() routine.
 *
 * To read the measurement, call get_speed() routine.
 *
 * You can dump the internal state to serial console using dump() method,
 * but this method is not synchronized (i.e., it may report erratic data).
 */
class FanRPM
{
public:
  enum {
    /// Minimum valid RPM.
    MIN_RPM = 60,
    /// Maximum valid RPM.
    MAX_RPM = 10000
  };

  /// Initialize the fan speed measurement.
  FanRPM() noexcept;

  /// Call this method in the interrupt function for the RPM measurement pin.
  void interrupt() noexcept;

  /*!
   * @brief Get the current speed measurement in rpm.
   */
  int getSpeed() noexcept;

  /// Dump the internal state to the serial console (unsynchronized read).
  void dump(Print& out) noexcept;

private:
  enum {
    /// Maximum # of measurements to store. Must be power of 2.
    MAX_MEASUREMENTS = 1<<5,
  };

  /// Last measurement time.
  unsigned long last_time_ = 0;
  /// Measurements buffer.
  unsigned long measurements_[MAX_MEASUREMENTS];
  /// Current measurement index. Wraps around the buffer.
  unsigned char index_ = 0;
  /// Set to true, if a valid measurement was found.
  volatile bool valid_ = false;
  /// Last measurement. Used to filter out outliers.
  unsigned long last_ = 0;
  /// Current sum of all measurements.
  volatile unsigned long sum_ = 0;
};
