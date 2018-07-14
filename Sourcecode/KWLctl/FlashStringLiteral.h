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
 * @brief Helper to create a Flash-stored string as a constexpr literal.
 */
#pragma once

#include <WString.h>

#include "cxx_support.h"

/*!
 * @brief Literal string type stored in Flash.
 *
 * Use makeFlashStringLiteral() to create an instance.
 */
template<unsigned len>
class PROGMEM FlashStringLiteral
{
public:
  /// Helper for converting to memory strings.
  class LoadHelper
  {
  public:
    /// Implicit conversion to const char*.
    inline operator const char*() const { return buffer_; }
  private:
    friend class FlashStringLiteral;
    inline LoadHelper(const char* p) { memcpy_P(buffer_, p, len); }
    char buffer_[len];
  };

  /// Construct the literal for given C string literal.
  constexpr FlashStringLiteral(const char (&s)[len]) :
    FlashStringLiteral(s, make_index_sequence<len>())
  {}

  /// Convert to helper, which is used to disambiguate const char* and Flash strings.
  inline operator const __FlashStringHelper*() const { return reinterpret_cast<const __FlashStringHelper*>(data_); }

  /// Get length of the string (without terminating NUL).
  constexpr size_t length() const { return len - 1; }

  /*!
   * @brief Load the string into memory and return it.
   *
   * @return helper object convertible to const char* which contains a copy
   *  of the string.
   *
   * @warning Pay attention to the lifecycle of the returned helper object.
   *  C++ will destroy the object at the end of the statement, if it is used
   *  inside of the statement as temporary. Put the returned object on the
   *  stack in an "auto" variable to extend the lifetime.
   */
  inline LoadHelper load() const {
    return LoadHelper(data_);
  }

private:
  template<unsigned... Indices>
  constexpr FlashStringLiteral(const char (&s)[len], index_sequence<Indices...>) :
    data_{s[Indices]...}
  {}

  /// This data resides in Flash.
  const char data_[len] PROGMEM;
};

/*!
 * @brief Construct a string literal residing in Flash memory.
 *
 * Typically, such literals will be used as constexpr constants like this:
 * @code
 * constexpr auto MyString = makeFlashLiteral("MyString");
 * @endcode
 *
 * Resulting literal is convertible to <tt>const __FlashStringHelper*</tt>,
 * so it can be used directly in routines expecting a string in Flash.
 * Using FlashStringLiteral::load() method, it is possible to create a temporary
 * on stack containing the string in RAM, so it can be passed as
 * <tt>const char*</tt> as necessary. Just pay attention to the lifecycle of
 * the temporary (destroyed at statement end, i.e., the returned buffer is
 * only valid until statement end).
 */
template<unsigned len>
constexpr FlashStringLiteral<len> makeFlashStringLiteral(const char (&s)[len]) {
  return FlashStringLiteral<len>(s);
}
