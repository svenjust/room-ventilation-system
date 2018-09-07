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
#include <string.h>

class HMS;

/*!
 * @brief Program data.
 */
class ProgramData
{
public:
  /// Bitmask with weekdays to run the program.
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
  /// Bitmask of program sets, in which this program is enabled.
  uint8_t enabled_progsets_;
  /// Reserved for future use (set to 0).
  uint8_t reserved_;

  /// Check whether this program is enabled.
  bool is_enabled(uint8_t progset_mask) const { return (enabled_progsets_ & progset_mask) != 0; }

  /// Enable or disable this program.
  void enable(uint8_t progset_mask) {
    enabled_progsets_ = progset_mask;
  }

  /// Check whether the time lies within the program. Assumes the program is enabled.
  bool matches(HMS hms) const;

  friend bool operator==(const ProgramData& l, const ProgramData& r) noexcept
  {
    return memcmp(&l, &r, sizeof(ProgramData)) == 0;
  }

  friend bool operator!=(const ProgramData& l, const ProgramData& r) noexcept
  {
    return memcmp(&l, &r, sizeof(ProgramData)) != 0;
  }
};
