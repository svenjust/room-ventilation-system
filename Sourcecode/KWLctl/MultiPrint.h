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
 * @brief Helper to print to two Print streams at the same time.
 */
#pragma once

#include <Arduino.h>

/*!
 * @brief Helper to print to two Print streams at the same time.
 *
 * Error codes and results are propagated from first stream only.
 * Errors on the second stream are ignored.
 */
class MultiPrint : public Print
{
public:
  MultiPrint(Print& s1, Print& s2) : s1_(s1), s2_(s2) {}
  virtual ~MultiPrint() {}

  virtual size_t write(uint8_t c);
  virtual size_t write(const uint8_t *buffer, size_t size);
  virtual int availableForWrite();
  virtual void flush();

private:
  Print& s1_;
  Print& s2_;
};
