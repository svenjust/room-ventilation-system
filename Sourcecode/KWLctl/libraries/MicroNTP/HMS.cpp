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

#include "HMS.h"

#include <Print.h>

HMS::HMS(uint32_t tm, int tz_offset, bool dst)
{
  if (!tm) {
    // invalid time
    h = m = s = wd = 0;
    return;
  }
  tm += static_cast<unsigned long>(static_cast<long>(tz_offset));
  if (dst)
    tm += 3600;
  s = tm % 60;
  tm /= 60;
  m = tm % 60;
  tm /= 60;
  h = tm % 24;
  tm /= 24;
  // 1.1.1970 was Thursday
  wd = (tm + 3) % 7;
}

void HMS::writeHM(char* buffer) const
{
  buffer[0] = char((h / 10) + '0');
  buffer[1] = char((h % 10) + '0');
  buffer[2] = ':';
  buffer[3] = char((m / 10) + '0');
  buffer[4] = char((m % 10) + '0');
}

void HMS::writeHMS(char* buffer) const
{
  writeHM(buffer);
  buffer[5] = ':';
  buffer[6] = char((s / 10) + '0');
  buffer[7] = char((s % 10) + '0');
}

size_t HMS::printTo(Print& p) const
{
  char buffer[8];
  writeHMS(buffer);
  return p.write(buffer, 8);
}

int HMS::compareTime(const HMS& other) const
{
  if (h < other.h) return -1;
  if (h > other.h) return 1;
  if (m < other.m) return -1;
  if (m > other.m) return 1;
  if (s < other.s) return -1;
  if (s > other.s) return 1;
  return 0;
}

int HMS::compare(const HMS& other) const
{
  if (wd < other.wd) return -1;
  if (wd > other.wd) return 1;
  return compareTime(other);
}

size_t PrintableHMS::printTo(Print& p) const
{
  return HMS::printTo(p);
}
