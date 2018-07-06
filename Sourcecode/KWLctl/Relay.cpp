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

#include "Relay.h"
#include "KWLConfig.h"

void Relay::set_mode(int8_t mode)
{
  switch (mode)
  {
  case HIGH:
  case LOW:
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, uint8_t(mode));
    break;

  default:
    pinMode(pin_, INPUT);
  }
}

void Relay::on()
{
  set_mode(KWLConfig::RelayON);
}

void Relay::off()
{
  set_mode(KWLConfig::RelayOFF);
}
