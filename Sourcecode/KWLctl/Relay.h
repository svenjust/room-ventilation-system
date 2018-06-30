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
 * @brief Relay control abstraction.
 */

#pragma once

#include <Arduino.h>

/*!
 * @brief Relay control abstraction.
 */
class Relay
{
public:
  /*!
   * @brief Construct relay control.
   *
   * @param pin digital output pin which controls the relay.
   */
  explicit Relay(uint8_t pin) : pin_(pin) {}

  /// Turn the relay on.
  void on();

  /// Turn the relay off.
  void off();

private:
  /// Set mode.
  inline void set_mode(int8_t mode);
  /// Digital output pin which controls the relay.
  uint8_t pin_;
};
