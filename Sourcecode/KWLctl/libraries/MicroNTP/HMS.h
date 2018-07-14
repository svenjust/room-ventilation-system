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
 * @brief Minimalistic time conversion utilities.
 */
#pragma once

#include <Printable.h>
#include <stdint.h>

class PrintableHMS;

/*!
 * @brief Minimalistic real time information.
 *
 * @note This class intentionally doesn't derive from Printable to keep it small.
 *    Convert to PrintableHMS to get a printable version.
 */
class HMS
{
public:
  HMS() = default;

  /*!
   * @brief Convert Unix time into hours/minutes/seconds + weekday.
   *
   * This method allows you to get the time "cooked" without any expensive
   * libraries, if you just need to get the time itself + optionally a weekday.
   *
   * @param tm Unix time (seconds since epoch).
   * @param tz_offset timezone offset in seconds (e.g., 3600 for GMT+1).
   * @param dst daylight saving time flag (summer time; adds 3600 to timezeone offset).
   * @return time parsed into hours/minutes/seconds + weekday.
   */
  HMS(uint32_t tm, int tz_offset, bool dst);

  /*!
   * @brief Construct time from its parts.
   *
   * @param ph,pm,ps time in hours, minutes and seconds.
   * @param pwd (optional) weekday (0 == Monday).
   */
  HMS(uint8_t ph, uint8_t pm, uint8_t ps = 0, uint8_t pwd = 0) :
    h(ph), m(pm), s(ps), wd(pwd)
  {}

  /// Print to a stream in form HH:MM:SS.
  size_t printTo(Print& p) const;

  /*!
   * @brief Write 5 bytes in form HH:MM into provided buffer.
   *
   * @param buffer buffer where to write, must have space for at least 5 chars.
   */
  void writeHM(char* buffer) const;

  /*!
   * @brief Write 8 bytes in form HH:MM:SS into provided buffer.
   *
   * @param buffer buffer where to write, must have space for at least 8 chars.
   */
  void writeHMS(char* buffer) const;

  /// Convert to a printable version implicitly.
  inline operator PrintableHMS() const;

  /*!
   * @brief Compare time (not weekday).
   *
   * @param other other time to compare with.
   * @return -1, 0 or 1, if this time is less than, equal or greater than other,
   *    respectively. Weekday is ignored when comparing.
   */
  int compareTime(const HMS& other) const;

  /*!
   * @brief Compare time including weekday.
   *
   * @param other other time to compare with.
   * @return -1, 0 or 1, if this time is less than, equal or greater than other,
   *    respectively.
   */
  int compare(const HMS& other) const;

  friend inline bool operator==(const HMS& l, const HMS& r) { return l.compare(r) == 0; }
  friend inline bool operator!=(const HMS& l, const HMS& r) { return l.compare(r) != 0; }
  friend inline bool operator<(const HMS& l, const HMS& r) { return l.compare(r) < 0; }
  friend inline bool operator>(const HMS& l, const HMS& r) { return l.compare(r) > 0; }

  /// Hours (0-23).
  uint8_t h = 0;
  /// Minutes (0-59).
  uint8_t m = 0;
  /// Seconds (0-59).
  uint8_t s = 0;
  /// Weekday (0-6, 0 == Monday).
  uint8_t wd = 0;
};

/// Printable version of HMS.
class PrintableHMS : public HMS, public Printable
{
public:
  PrintableHMS(const HMS& other) : HMS(other) {}

  virtual size_t printTo(Print& p) const override;
};

inline HMS::operator PrintableHMS() const {
  return PrintableHMS(*this);
}
