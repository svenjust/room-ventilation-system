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
 * @brief Helper to ease string comparisons.
 */
#pragma once

#include <WString.h>
#include <stdlib.h>

/*!
 * @brief Helper to ease string comparisons.
 */
class StringView
{
public:
  StringView() : StringView("", 0) {}
  StringView(const char* string) : data_(string), length_(strlen(string)) {}
  StringView(const char* string, unsigned int length) : data_(string), length_(length) {}

  const char* c_str() const { return data_; }
  unsigned int length() const { return length_; }

  bool operator==(const char* other) const {
    if (memcmp(data_, other, length_) != 0)
      return false;
    return other[length_] == 0;
  }

  bool operator==(const __FlashStringHelper* other) const {
    if (memcmp_P(data_, other, length_) != 0)
      return false;
    return pgm_read_byte(reinterpret_cast<const uint8_t*>(other) + length_) == 0;
  }

  bool operator==(const StringView& other) const {
    if (length_ != other.length_)
      return false;
    return 0 == memcmp(data_, other.data_, length_);
  }

  long toInt() const {
    return atol(data_);
  }
  float toFloat() const {
    return float(atof(data_));
  }
  double toDouble() const {
    return atof(data_);
  }

private:
  const char* data_;
  unsigned int length_;
};
