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
 * @brief MQTT topics for communication with outside world.
 */

#pragma once

#include "FlashStringLiteral.h"

/*!
 * @brief MQTT Topics für die Kommunikation zwischen dieser Steuerung und einem mqtt Broker.
 *
 * Die Topics sind in https://github.com/svenjust/room-ventilation-system/blob/master/Docs/mqtt%20topics/mqtt_topics.ods erläutert.
 */
namespace MQTTTopic
{
  constexpr auto Command                    = makeFlashStringLiteral("d15/set/#");
  constexpr auto CommandDebug               = makeFlashStringLiteral("d15/debugset/#");
  constexpr auto CmdResetAll                = makeFlashStringLiteral("d15/set/kwl/resetAll_IKNOWWHATIMDOING");
  constexpr auto CmdCalibrateFans           = makeFlashStringLiteral("d15/set/kwl/calibratefans");
  constexpr auto CmdFansCalculateSpeedMode  = makeFlashStringLiteral("d15/set/kwl/fans/calculatespeed");
  constexpr auto CmdFan1Speed               = makeFlashStringLiteral("d15/set/kwl/fan1/standardspeed");
  constexpr auto CmdFan2Speed               = makeFlashStringLiteral("d15/set/kwl/fan2/standardspeed");
  constexpr auto CmdGetSpeed                = makeFlashStringLiteral("d15/set/kwl/fans/getspeed");
  constexpr auto CmdGetTemp                 = makeFlashStringLiteral("d15/set/kwl/temperatur/gettemp");
  constexpr auto CmdGetvalues               = makeFlashStringLiteral("d15/set/kwl/getvalues");
  constexpr auto CmdMode                    = makeFlashStringLiteral("d15/set/kwl/lueftungsstufe");
  constexpr auto CmdAntiFreezeHyst          = makeFlashStringLiteral("d15/set/kwl/antifreeze/hysterese");
  constexpr auto CmdBypassGetValues         = makeFlashStringLiteral("d15/set/kwl/summerbypass/getvalues");
  constexpr auto CmdBypassManualFlap        = makeFlashStringLiteral("d15/set/kwl/summerbypass/flap");
  constexpr auto CmdBypassMode              = makeFlashStringLiteral("d15/set/kwl/summerbypass/mode");
  constexpr auto CmdBypassHystereseMinutes  = makeFlashStringLiteral("d15/set/kwl/summerbypass/HystereseMinutes");
  constexpr auto CmdHeatingAppCombUse       = makeFlashStringLiteral("d15/set/kwl/heatingapp/combinedUse");
  constexpr auto CmdSetProgram              = makeFlashStringLiteral("d15/set/kwl/setProgram");
  constexpr auto CmdGetProgram              = makeFlashStringLiteral("d15/set/kwl/getProgram");

  constexpr auto Heartbeat                  = makeFlashStringLiteral("d15/state/kwl/heartbeat");
  constexpr auto Fan1Speed                  = makeFlashStringLiteral("d15/state/kwl/fan1/speed");
  constexpr auto Fan2Speed                  = makeFlashStringLiteral("d15/state/kwl/fan2/speed");
  constexpr auto KwlOnline                  = makeFlashStringLiteral("d15/state/kwl/heartbeat");
  constexpr auto StateKwlMode               = makeFlashStringLiteral("d15/state/kwl/lueftungsstufe");
  constexpr auto KwlTemperaturAussenluft    = makeFlashStringLiteral("d15/state/kwl/aussenluft/temperatur");
  constexpr auto KwlTemperaturZuluft        = makeFlashStringLiteral("d15/state/kwl/zuluft/temperatur");
  constexpr auto KwlTemperaturAbluft        = makeFlashStringLiteral("d15/state/kwl/abluft/temperatur");
  constexpr auto KwlTemperaturFortluft      = makeFlashStringLiteral("d15/state/kwl/fortluft/temperatur");
  constexpr auto KwlEffiency                = makeFlashStringLiteral("d15/state/kwl/effiencyKwl");
  constexpr auto KwlAntifreeze              = makeFlashStringLiteral("d15/state/kwl/antifreeze");
  constexpr auto KwlBypassState             = makeFlashStringLiteral("d15/state/kwl/summerbypass/flap");
  constexpr auto KwlBypassMode              = makeFlashStringLiteral("d15/state/kwl/summerbypass/mode");
  constexpr auto KwlBypassTempAbluftMin     = makeFlashStringLiteral("d15/state/kwl/summerbypass/TempAbluftMin");
  constexpr auto KwlBypassTempAussenluftMin = makeFlashStringLiteral("d15/state/kwl/summerbypass/TempAussenluftMin");
  constexpr auto KwlBypassHystereseMinutes  = makeFlashStringLiteral("d15/state/kwl/summerbypass/HystereseMinutes");
  constexpr auto KwlHeatingAppCombUse       = makeFlashStringLiteral("d15/state/kwl/heatingapp/combinedUse");
  constexpr auto KwlProgram                 = makeFlashStringLiteral("d15/state/kwl/program");

  constexpr auto KwlDHT1Temperatur          = makeFlashStringLiteral("d15/state/kwl/dht1/temperatur");
  constexpr auto KwlDHT2Temperatur          = makeFlashStringLiteral("d15/state/kwl/dht2/temperatur");
  constexpr auto KwlDHT1Humidity            = makeFlashStringLiteral("d15/state/kwl/dht1/humidity");
  constexpr auto KwlDHT2Humidity            = makeFlashStringLiteral("d15/state/kwl/dht2/humidity");
  constexpr auto KwlCO2Abluft               = makeFlashStringLiteral("d15/state/kwl/abluft/co2");
  constexpr auto KwlVOCAbluft               = makeFlashStringLiteral("d15/state/kwl/abluft/voc");


  // Die folgenden Topics sind nur für die SW-Entwicklung, und schalten Debugausgaben per mqtt ein und aus
  constexpr auto KwlDebugsetFan1Getvalues   = makeFlashStringLiteral("d15/debugset/kwl/fan1/getvalues");
  constexpr auto KwlDebugsetFan2Getvalues   = makeFlashStringLiteral("d15/debugset/kwl/fan2/getvalues");
  constexpr auto KwlDebugstateFan1          = makeFlashStringLiteral("d15/debugstate/kwl/fan1");
  constexpr auto KwlDebugstateFan2          = makeFlashStringLiteral("d15/debugstate/kwl/fan2");
  constexpr auto KwlDebugstatePreheater     = makeFlashStringLiteral("d15/debugstate/kwl/preheater");
  constexpr auto KwlDebugsetSchedulerGetvalues   = makeFlashStringLiteral("d15/debugset/kwl/scheduler/getvalues");
  constexpr auto KwlDebugsetSchedulerResetvalues = makeFlashStringLiteral("d15/debugset/kwl/scheduler/resetvalues");
  constexpr auto KwlDebugstateScheduler    = makeFlashStringLiteral("d15/debugstate/kwl/scheduler");

  // Die folgenden Topics sind nur für die SW-Entwicklung, es werden Messwerte überschrieben, es kann damit der Sommer-Bypass und die Frostschutzschaltung getestet werden
  constexpr auto KwlDebugsetTemperaturAussenluft = makeFlashStringLiteral("d15/debugset/kwl/aussenluft/temperatur");
  constexpr auto KwlDebugsetTemperaturZuluft     = makeFlashStringLiteral("d15/debugset/kwl/zuluft/temperatur");
  constexpr auto KwlDebugsetTemperaturAbluft     = makeFlashStringLiteral("d15/debugset/kwl/abluft/temperatur");
  constexpr auto KwlDebugsetTemperaturFortluft   = makeFlashStringLiteral("d15/debugset/kwl/fortluft/temperatur");

  // Die folgenden Topics sind nur für die SW-Entwicklung, um Kalibrierung explizit zu setzen
  constexpr auto KwlDebugsetFan1PWM         = makeFlashStringLiteral("d15/debugset/kwl/fan1/pwm");
  constexpr auto KwlDebugsetFan2PWM         = makeFlashStringLiteral("d15/debugset/kwl/fan2/pwm");
  constexpr auto KwlDebugsetFanPWMStore     = makeFlashStringLiteral("d15/debugset/kwl/fan/pwm/store_IKNOWWHATIMDOING");
}
