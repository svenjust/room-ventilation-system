/*
 * Copyright (C) 2018 Sven Just (sven@familie-just.de)
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
 * @brief Additional sensors of the ventilation system (optional).
 */
#pragma once

#include "Task.h"

class Print;

/*!
 * @brief Additional sensors of the ventilation system (optional).
 *
 * This class reads and publishes values of additional sensors like
 * humidity, CO2 and VOC.
 */
class AdditionalSensors : private Task
{
public:
  AdditionalSensors();

  /// Initialize sensors.
  void begin(Print& initTracer);

  /// Force sending values via MQTT on the next MQTT run.
  void forceSend();

private:
  /// Handle the tasks (timed).
  void run();
};
