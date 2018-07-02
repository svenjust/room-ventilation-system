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
 * @brief Helper to print trace during initialization of a class.
 */
#pragma once

class Print;
class __FlashStringHelper;

/*!
 * @brief Helper to print trace during initialization of a class.
 *
 * Make this the first base class of another class. Then, the message will be
 * printed before the initialization using out.println().
 */
class InitTrace
{
public:
  /*!
   * @brief Print trace message in constructor.
   *
   * @param msg message to print.
   * @param out output where to print.
   */
  InitTrace(const char* msg, Print& out);

  /*!
   * @brief Print trace message in constructor.
   *
   * @param msg message to print.
   * @param out output where to print.
   */
  InitTrace(const __FlashStringHelper* msg, Print& out);
};
