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
 * by sending a signal once per rotation (by default; other signal frequencies
 * are possible, see multiplier()). The signal is captured from a pin
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
  /// Type for storing fan multiplier.
  using multiplier_t = unsigned char;

  enum {
    /// Minimum valid RPM.
    MIN_RPM = 60,
    /// Maximum valid RPM.
    MAX_RPM = 10000,
    /// Multiplier representing 1 tacho signal per rotation.
    RPM_MULTIPLIER_BASE = 100
    // NOTE: see usages for these values if you intend to change them, you may
    // need to adjust RPM and timing computation.
  };

  /*!
   * @brief Initialize the fan speed measurement.
   *
   * @param multiplier (optional) multiplier to set, if the tacho signal is
   *    not sent 1:1 for each rotation. It is set in units of
   *    1/RPM_MULTIPLIER_BASE.
   * @see multiplier()
   */
  FanRPM(multiplier_t multiplier = RPM_MULTIPLIER_BASE) noexcept;

  /*!
   * @brief Get or set the multiplier for computing RPM.
   *
   * The multiplier is used to compute real RPM of the fan. There are fans,
   * which do not send one tacho signal per rotation, but rather 2 or 3 or 4.
   * Setting the multiplier to proper value will correct the measurement.
   *
   * The multiplier is set in units of 1/RPM_MULTIPLIER_BASE. I.e., the value
   * RPM_MULTIPLIER_BASE means identity, or 1:1 speed. Multiplier of RPM_MULTIPLIER_BASE/2
   * would mean 1/2, i.e., when the fan sends 2 signals per rotation, use this multiplier.
   *
   * The change of the multiplier takes effect immediately.
   *
   * @return reference to multiplier.
   */
  multiplier_t& multiplier() noexcept { return multiplier_; }

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
  /// Multiplier in 1/256 units to convert to real RPM.
  multiplier_t multiplier_ = RPM_MULTIPLIER_BASE;
};
