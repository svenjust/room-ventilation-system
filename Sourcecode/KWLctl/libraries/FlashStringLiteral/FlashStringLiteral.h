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

/// Utility functions from C++11/14 standard to implement Flash string literal.
namespace FlashStringImpl
{
  template<class T, T... Ints> struct integer_sequence {};

  namespace impl
  {
    template<class S> struct integer_sequence_extend;

    template<class T, T... Ints> struct integer_sequence_extend<integer_sequence<T, Ints...>>
    {
        using type = integer_sequence<T, Ints..., sizeof...(Ints)>;
    };

    template<class T, T I, T N> struct make_integer_sequence_helper;

    template<class T, T I, T N> struct make_integer_sequence_helper
    {
        using type = typename integer_sequence_extend<
            typename make_integer_sequence_helper<T, I+1, N>::type>::type;
    };

    template<class T, T N> struct make_integer_sequence_helper<T, N, N>
    {
        using type = integer_sequence<T>;
    };
  }

  template<class T, T N>
  using make_integer_sequence = typename impl::make_integer_sequence_helper<T, 0, N>::type;

  template<unsigned... Ints>
  using index_sequence = integer_sequence<unsigned, Ints...>;

  template<unsigned N>
  using make_index_sequence = make_integer_sequence<unsigned, N>;
}

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
    /// Implicit conversion to char*.
    inline operator char*() noexcept { return buffer_; }
    /// Implicit conversion to const char*.
    inline operator const char*() const noexcept { return buffer_; }
  private:
    friend class FlashStringLiteral;
    inline LoadHelper(const char* p) noexcept { memcpy_P(buffer_, p, len); }
    char buffer_[len];
  };

  /// Construct the literal for given C string literal.
  constexpr FlashStringLiteral(const char (&s)[len]) noexcept :
    FlashStringLiteral(s, FlashStringImpl::make_index_sequence<len>())
  {}

  /// Convert to helper, which is used to disambiguate const char* and Flash strings.
  inline operator const __FlashStringHelper*() const noexcept { return reinterpret_cast<const __FlashStringHelper*>(data_); }

  /// Get length of the string (without terminating NUL).
  constexpr size_t length() const noexcept { return len - 1; }

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
  inline LoadHelper load() const noexcept {
    return LoadHelper(data_);
  }

  /*!
   * @brief Store the string into provided buffer.
   *
   * @param buffer buffer to store to. It must have a size of at least length() + 1.
   */
  inline void store(char* buffer) const noexcept {
    memcpy_P(buffer, data_, len);
  }

private:
  template<unsigned... Indices>
  constexpr FlashStringLiteral(const char (&s)[len], FlashStringImpl::index_sequence<Indices...>) noexcept :
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
