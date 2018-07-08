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

#include "PersistentConfiguration.h"

#include <EEPROM.h>
#include <Print.h>

/// Lowest possible EEPROM address.
static constexpr int EEPROM_MIN_ADDR = 0;
/// Address past addressable EEPROM contents.
static constexpr int EEPROM_MAX_ADDR = 1024;

void PersistentConfigurationBase::begin(Print& out, unsigned int size, unsigned int version, LoadFnc load_defaults, bool reset)
{
  out.println(F("Reading EEPROM contents..."));
  dumpRaw(out);

  // read the config from EEPROM
  uint8_t* data = reinterpret_cast<uint8_t*>(this);
  int limit = int(EEPROM_MIN_ADDR + size);
  if (limit > EEPROM_MAX_ADDR) {
    limit = EEPROM_MAX_ADDR;
    out.println(F("ERROR: Persistent configuration size exceeds available EEPROM space"));
  }
  for (int i = EEPROM_MIN_ADDR; i < limit; ++i)
    *data++ = EEPROM.read(i);

  if (version_ != version) {
    out.print(F("Invalid configuration version found: "));
    out.print(version_);
    out.print(F(", expected "));
    out.print(version);
    out.println(F(". Loading defaults into EEPROM."));
    reset = true;
  } else if (reset) {
    out.println(F("WARNING: EEPROM reset requested, loading defaults into EEPROM."));
  }

  if (reset) {
    (this->*load_defaults)();
    version_ = version;

    out.println(F("Writing EEPROM contents..."));
    updateRange(this, size);

    // clear the rest
    for (int i = int(size); i < EEPROM_MAX_ADDR; ++i)
      EEPROM.update(i, uint8_t(0xff));

    // verify that the configuration matches
    data = reinterpret_cast<uint8_t*>(this);
    bool error = false;
    for (int i = EEPROM_MIN_ADDR; i < limit && !error; ++i)
      error = *data++ != EEPROM.read(i);
    if (error) {
      out.println(F("ERROR: EEPROM couldn't be written correctly, will reset and retry on next startup."));
      factoryReset();
    } else {
      out.println(F("New EEPROM contents stored successfully."));
    }
  }
}

void PersistentConfigurationBase::factoryReset()
{
  version_ = 0;
  update(version_);
}

bool PersistentConfigurationBase::updateRange(const void* ptr, unsigned int size) const {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(ptr);
  int addr = data - reinterpret_cast<const uint8_t*>(this);
  int limit = addr + int(size);
  if (addr < EEPROM_MIN_ADDR)
    return false;
  if (limit > EEPROM_MAX_ADDR)
    return false;
  while (addr < limit)
    EEPROM.update(addr++, *data++);
  return true;
}

void PersistentConfigurationBase::dumpRaw(Print& out, unsigned bytes_per_row)
{
  unsigned j = 0;
  char buf[10];

  for (int i = EEPROM_MIN_ADDR; i < EEPROM_MAX_ADDR; i++) {
    if (j == 0) {
      sprintf(buf, "%03X:", i);
      out.print(buf);
    }
    uint8_t b = EEPROM.read(i);
    sprintf(buf, " %02X", b);
    j++;
    if (j == bytes_per_row) {
      j = 0;
      out.println(buf);
    } else {
      out.print(buf);
    }
  }
  if (j)
    out.println();
}

