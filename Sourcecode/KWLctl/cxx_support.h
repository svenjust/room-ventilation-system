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
 * @brief Various compatibility stuff with C++11/14 std library.
 *
 * Currently defines:
 *    - integer_sequence and make_integer_sequence()
 *    - index_sequence and make_index_sequence()
 */
#pragma once

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

inline void *operator new(unsigned /*size*/, void *ptr)
{
    return ptr;
}

