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
 * @brief Simple screenshot service writing bitmap with TFT contents.
 */
#pragma once

class MCUFRIEND_kbv;
class Print;

/*!
 * @brief Simple screenshot service writing bitmap with TFT contents.
 */
class ScreenshotService
{
public:
  /// Make screenshot and print it into the stream.
  static void make(MCUFRIEND_kbv& tft, Print& client) noexcept;
};

