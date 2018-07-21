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
  StringView() noexcept : StringView("", 0) {}
  StringView(const char* string) noexcept : data_(string), length_(strlen(string)) {}
  StringView(const char* string, unsigned int length) noexcept : data_(string), length_(length) {}

  StringView& operator=(const StringView& other) noexcept {
    data_ = other.data_;
    length_ = other.length_;
    return *this;
  }

  const char* c_str() const noexcept { return data_; }
  unsigned int length() const noexcept { return length_; }

  bool operator==(const char* other) const noexcept {
    if (memcmp(data_, other, length_) != 0)
      return false;
    return other[length_] == 0;
  }

  bool operator==(const __FlashStringHelper* other) const noexcept {
    if (memcmp_P(data_, other, length_) != 0)
      return false;
    return pgm_read_byte(reinterpret_cast<const uint8_t*>(other) + length_) == 0;
  }

  bool operator==(const StringView& other) const noexcept {
    if (length_ != other.length_)
      return false;
    return 0 == memcmp(data_, other.data_, length_);
  }

  bool operator!=(const char* other) const noexcept { return !operator==(other); }
  bool operator!=(const __FlashStringHelper* other) const noexcept { return !operator==(other); }
  bool operator!=(const StringView& other) const noexcept { return !operator==(other); }

  long toInt() const noexcept {
    return atol(data_);
  }
  float toFloat() const noexcept {
    return float(atof(data_));
  }
  double toDouble() const noexcept {
    return atof(data_);
  }

  /*!
   * @brief Return a substring.
   *
   * @param pos first character position of the substring.
   * @param count count of characters.
   * @return substring starting at pos of length of at most count characters.
   */
  StringView substr(size_t pos = 0, size_t count = size_t(-1)) const noexcept {
    if (pos >= length_)
      return StringView();
    size_t rem = length_ - pos;
    if (count > rem)
      count = rem;
    return StringView(data_ + pos, count);
  }

private:
  const char* data_;
  size_t length_;
};
