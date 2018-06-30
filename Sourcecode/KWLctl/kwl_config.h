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
 * @brief Configuration of the project.
 *
 * @note DO NOT MODIFY THIS FILE. Put your configuration into user_config.h.
 */

#pragma once

#include <Arduino.h>

/// Default configuration, which can be overridden by the user configuration below.
class kwl_default_config
{
protected:
  /// Helper for relay state.
  static const int8_t OPEN = -1;

public:

  // ***************************************************  A N S C H L U S S E I N S T E L L U N G E N ***************************************************
  // Die Anschlüsse können dem Schaltplan entnommen werden: https://github.com/svenjust/room-ventilation-system/blob/master/Docs/circuit%20diagram/KWL-Steuerung%20f%C3%BCr%20P300.pdf

  /// Bypass Strom an/aus. Das BypassPower steuert, ob Strom am Bypass geschaltet ist, BypassDirection bestimmt Öffnen oder Schliessen.
  static const uint8_t PinBypassPower     = 40;
  /// Bypass Richtung, Stromlos = Schliessen (Winterstellung), Strom = Öffnen (Sommerstellung).
  static const uint8_t PinBypassDirection = 41;
  /// Stromversorgung Lüfter 1.
  static const uint8_t PinFan1Power       = 42;
  /// Stromversorgung Lüfter 2.
  static const uint8_t PinFan2Power       = 43;

  // *******************************************E N D E ***  A N S C H L U S S E I N S T E L L U N G E N ***************************************************

  // ************************************** A N S T E U E R U N G   D E R    R E L A I S ************************************************************************
  // Für die Lüfter und den Sommer-Bypass können bis zu vier Relais verbaut sein.
  // Ohne Sommer-Bypass kann die Schaltung auch ohne Relais betrieben werden.
  // Da verschiedene Relais unterschiedlich geschaltet werden, kann hier die logische
  // Schaltung definiert werden (LOW, HIGH oder OPEN für high-impedance mode).

  /// Relay pin digital output needed to close the relay (HIGH/LOW/OPEN).
  static const int8_t RelayON = LOW;
  /// Relay pin digital output needed to open the relay (HIGH/LOW/OPEN).
  static const int8_t RelayOFF = HIGH;

  // ************************************** E N D E   A N S T E U E R U N G   D E R    R E L A I S ***************************************************************

};

/// Helper macro to add configuration in user_config.h.
#define CONFIGURE(name, value) static const decltype(kwl_default_config::name) name = value;

/// Actual configuration, including user-specific values.
class kwl_config : public kwl_default_config
{
public:
#include "user_config.h"
};
