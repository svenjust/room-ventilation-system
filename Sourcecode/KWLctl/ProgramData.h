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
 * @brief Program data.
 */
#pragma once

#include <stdint.h>

class HMS;

/*!
 * @brief Program data.
 */
class ProgramData
{
public:
  /// Validity flag stored in high bit of weekdays.
  static constexpr uint8_t VALID_FLAG = 0x80;

  /// Bitmask with weekdays to run the program. Flag in high bit to indicate validity.
  uint8_t weekdays_;
  /// Fan mode to set.
  uint8_t fan_mode_;
  /// Start hour.
  uint8_t start_h_;
  /// Start minute.
  uint8_t start_m_;
  /// End hour.
  uint8_t end_h_;
  /// End minute.
  uint8_t end_m_;
  /// Reserved for future use (set to 0).
  uint16_t reserved_;

  /// Check whether this program is enabled.
  bool enabled() const { return (weekdays_ & VALID_FLAG) != 0; }

  /// Enable or disable this program.
  void enable(bool enabled) {
    if (enabled)
      weekdays_ |= VALID_FLAG;
    else
      weekdays_ &= ~VALID_FLAG;
  }

  /// Check whether the time lies within the program.
  bool matches(HMS hms) const;
};
