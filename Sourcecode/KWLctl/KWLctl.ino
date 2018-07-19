/*
  ################################################################
  #
  #   Copyright notice
  #
  #   Control software for a Room Ventilation System
  #   https://github.com/svenjust/room-ventilation-system
  #
  #   Copyright (C) 2018  Sven Just (sven@familie-just.de)
  #
  #   This program is free software: you can redistribute it and/or modify
  #   it under the terms of the GNU General Public License as published by
  #   the Free Software Foundation, either version 3 of the License, or
  #   (at your option) any later version.
  #
  #   This program is distributed in the hope that it will be useful,
  #   but WITHOUT ANY WARRANTY; without even the implied warranty of
  #   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  #   GNU General Public License for more details.
  #
  #   You should have received a copy of the GNU General Public License
  #   along with this program.  If not, see <https://www.gnu.org/licenses/>.
  #
  #   This copyright notice MUST APPEAR in all copies of the script!
  #
  ################################################################
*/

/*
  # Steuerung einer Lüftungsanlage für Wohnhäuser

  Diese Steuerung wurde als Ersatz der Originalsteuerung entwickelt.

  Das Original war Pluggit P300. Diese Steuerung funktioniert ebenso für P450 und viele weitere Lüftungsanlagen.

  Es werden:
  1. zwei Lüfter angesteuert und deren Tachosignale ausgelesen
  2. vier Temperatursensoren abgefragt
  3. Steuerung Abluftventilator zum Frostschutz
  4. Optional: Vorheizregister als Frostschutz
  5. Optional: Sommer-Bypass geschaltet
  6. Optional: Vorheizregister als Frostschutz
  Die Drehzahlregelung der Lüftermotoren erfolgt durch PID-Regler. Dies sorgt dafür, das die geforderten Drehzahlen der Lüfter sicher erreicht werden. Die Leistungssteuerung des Vorheizregisters erfolgt durch einen PID-Regler.

  Differenzdrucksensoren werden bisher nicht berücksichtigt.

  Die Steuerung ist per LAN (W5100) erreichbar. Als Protokoll wird mqtt verwendet. Über mqtt liefert die Steuerung Temperaturen, Drehzahlen der Lüfter, Stellung der Bypassklappe und den Status der AntiFreeze Funktion zurück.
*/

#include "KWLControl.hpp"

#include <MCUFRIEND_kbv.h>      // TFT
#include <Wire.h>
#include <MultiPrint.h>
#include <avr/wdt.h>

// Actual instance of the control system.
KWLControl kwlControl;

static void loopWrite100Millis() {
  auto currentMillis = millis();
  static unsigned long previous100Millis = 0;
  if (currentMillis - previous100Millis > 100) {
    previous100Millis = currentMillis;
    Serial.print (F("Timestamp: "));
    Serial.println(long(currentMillis));
  }
}

/**********************************************************************
  Setup Routinen
**********************************************************************/

// External references to TFT
extern void SetupTftScreen();
extern void SetupTouch();
extern void SetupBackgroundScreen();
extern MCUFRIEND_kbv tft;

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

// *** SETUP START ***
void setup()
{

  Serial.begin(57600); // Serielle Ausgabe starten

  if (mcusr_mirror) {
    Serial.print(F("WARNING: System was reset by watchdog, code="));
    Serial.println(mcusr_mirror);
  }

  // *** TFT AUSGABE ***
  SetupTftScreen();
  SetupTouch();

  // Init tracer which prints to both TFT and Serial.
  static MultiPrint initTracer(Serial, tft);

  initTracer.println(F("Booting... "));

  if (KWLConfig::ControlFansDAC) {
    // TODO Also if using Preheater DAC, but no Fan DAC
    Wire.begin();               // I2C-Pins definieren
    initTracer.println(F("Initialisierung DAC"));
  }

  kwlControl.begin(initTracer);

  Serial.println();

  if (kwlControl.getAntifreeze().getHeatingAppCombUse()) {
    initTracer.println(F("...System mit Feuerstaettenbetrieb"));
  }

  // Setup fertig
  initTracer.println(F("Setup completed..."));

  // 4 Sekunden Pause für die TFT Anzeige, damit man sie auch lesen kann
  delay (4000);

  SetupBackgroundScreen();   // Bootmeldungen löschen, Hintergrund für Standardanzeige starten

  // in case no loop is executed for 8 seconds, trigger watchdog (reboot).
  wdt_enable(WDTO_8S);
}
// *** SETUP ENDE

// *** LOOP START ***
void loop()
{
  Task::loop();
  //loopWrite100Millis();
  wdt_reset();
}
// *** LOOP ENDE ***
