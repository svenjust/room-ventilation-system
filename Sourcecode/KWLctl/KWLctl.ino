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

#include <MultiPrint.h>

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

// *** SETUP START ***
void setup()
{
  Serial.begin(57600); // Serielle Ausgabe starten

  // *** TFT AUSGABE ***
  kwlControl.getTFT().setup();

  // Init tracer which prints to both TFT and Serial.
  static MultiPrint initTracer(Serial, kwlControl.getTFT().getTFTPrinter());

  initTracer.println(F("Booting... "));

  kwlControl.begin(initTracer);
}
// *** SETUP ENDE

// *** LOOP START ***
void loop()
{
  kwlControl.loop();
  //loopWrite100Millis();
}
// *** LOOP ENDE ***
