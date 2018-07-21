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

#include "DeadlockWatchdog.h"

#include <avr/wdt.h>
#include <Arduino.h>

/// Reportig function.
static DeadlockWatchdog::report_fnc s_report_fnc = nullptr;
/// Argument for reporting function.
static void* s_fnc_arg = nullptr;

/// Mirror of watchdog reset source.
static uint8_t mcusr_mirror __attribute__ ((section (".noinit")));

// Disable watchdog after reset, as required by ATmega datasheet (and documented in wdt.h)
void get_mcusr(void) \
  __attribute__((naked)) \
  __attribute__((section(".init3")));
void get_mcusr(void)
{
  mcusr_mirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

/*
Function called when the watchdog interrupt fires. The function is naked so that
we don't get program state pushed onto the stack. Consequently the top three
bytes on the stack will be the program counter when the interrupt fired. We're
going to save that in the eeprom. We never return from this function.
*/
static uint8_t *crash_sp;
extern "C" {
  void watchdog_isr_gate(void) __attribute__((used));
  void watchdog_isr_gate(void) {
    // The stack pointer on the AVR micro points to the next available location
    // so we want to go back one location to get the first byte of the address
    // pushed onto the stack when the interrupt was triggered. There will be
    // PROGRAM_COUNTER_SIZE bytes there.
    // NOTE: AVR is little endian, but return address is stored as big endian. Convert.
    uint32_t pc = (uint32_t(crash_sp[1]) << 16) | (uint16_t(crash_sp[2]) << 8) | crash_sp[3];
    s_report_fnc(pc, reinterpret_cast<unsigned>(crash_sp) + 3, s_fnc_arg);
    Serial.print(F("Resetting, crash detected at 0x"));
    Serial.println(pc * 2, HEX);
    Serial.flush();
    __asm__ __volatile__ (
      "jmp 0 \n"
    );
  }
}

ISR(WDT_vect, ISR_NAKED)
{
  // Store a pointer to the program counter.
  crash_sp = reinterpret_cast<uint8_t*>(SP);

  // Newer versions of GCC don't like when naked functions call regular functions
  // so we jump to a gate function instead
  __asm__ __volatile__ (
    "jmp watchdog_isr_gate \n"
  );
}

void DeadlockWatchdog::begin(report_fnc f, void* f_arg) noexcept
{
  begin(f, WDTO_8S, f_arg);
}

void DeadlockWatchdog::begin(report_fnc f, unsigned char to, void* f_arg) noexcept
{
  // in case no loop is executed for 8 seconds, trigger watchdog (store crash location and reboot).
  wdt_enable(to);
  WDTCSR |= _BV(WDIE);
  s_report_fnc = f;
  s_fnc_arg = f_arg;

  if (mcusr_mirror) {
    Serial.print(F("WARNING: System was reset by watchdog, code="));
    Serial.println(mcusr_mirror);
  }
}

void DeadlockWatchdog::reset() noexcept
{
  wdt_reset();
}

void DeadlockWatchdog::disable() noexcept
{
  wdt_disable();
}




