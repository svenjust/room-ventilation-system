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

/*!
 * @brief MQTT Topics für die Kommunikation zwischen dieser Steuerung und einem mqtt Broker.
 *
 * Die Topics sind in https://github.com/svenjust/room-ventilation-system/blob/master/Docs/mqtt%20topics/mqtt_topics.ods erläutert.
 */
namespace MQTTTopic
{
  constexpr const char* Command                    = "d15/set/#";
  constexpr const char* CommandDebug               = "d15/debugset/#";
  constexpr const char* CmdResetAll                = "d15/set/kwl/resetAll_IKNOWWHATIMDOING";
  constexpr const char* CmdCalibrateFans           = "d15/set/kwl/calibratefans";
  constexpr const char* CmdFansCalculateSpeedMode  = "d15/set/kwl/fans/calculatespeed";
  constexpr const char* CmdFan1Speed               = "d15/set/kwl/fan1/standardspeed";
  constexpr const char* CmdFan2Speed               = "d15/set/kwl/fan2/standardspeed";
  constexpr const char* CmdGetSpeed                = "d15/set/kwl/fans/getspeed";
  constexpr const char* CmdGetTemp                 = "d15/set/kwl/temperatur/gettemp";
  constexpr const char* CmdGetvalues               = "d15/set/kwl/getvalues";
  constexpr const char* CmdMode                    = "d15/set/kwl/lueftungsstufe";
  constexpr const char* CmdAntiFreezeHyst          = "d15/set/kwl/antifreeze/hysterese";
  constexpr const char* CmdBypassGetValues         = "d15/set/kwl/summerbypass/getvalues";
  constexpr const char* CmdBypassManualFlap        = "d15/set/kwl/summerbypass/flap";
  constexpr const char* CmdBypassMode              = "d15/set/kwl/summerbypass/mode";
  constexpr const char* CmdBypassHystereseMinutes  = "d15/set/kwl/summerbypass/HystereseMinutes";
  constexpr const char* CmdHeatingAppCombUse       = "d15/set/kwl/heatingapp/combinedUse";

  constexpr const char* Heartbeat                  = "d15/state/kwl/heartbeat";
  constexpr const char* Fan1Speed                  = "d15/state/kwl/fan1/speed";
  constexpr const char* Fan2Speed                  = "d15/state/kwl/fan2/speed";
  constexpr const char* KwlOnline                  = "d15/state/kwl/heartbeat";
  constexpr const char* StateKwlMode               = "d15/state/kwl/lueftungsstufe";
  constexpr const char* KwlTemperaturAussenluft    = "d15/state/kwl/aussenluft/temperatur";
  constexpr const char* KwlTemperaturZuluft        = "d15/state/kwl/zuluft/temperatur";
  constexpr const char* KwlTemperaturAbluft        = "d15/state/kwl/abluft/temperatur";
  constexpr const char* KwlTemperaturFortluft      = "d15/state/kwl/fortluft/temperatur";
  constexpr const char* KwlEffiency                = "d15/state/kwl/effiencyKwl";
  constexpr const char* KwlAntifreeze              = "d15/state/kwl/antifreeze";
  constexpr const char* KwlBypassState             = "d15/state/kwl/summerbypass/flap";
  constexpr const char* KwlBypassMode              = "d15/state/kwl/summerbypass/mode";
  constexpr const char* KwlBypassTempAbluftMin     = "d15/state/kwl/summerbypass/TempAbluftMin";
  constexpr const char* KwlBypassTempAussenluftMin = "d15/state/kwl/summerbypass/TempAussenluftMin";
  constexpr const char* KwlBypassHystereseMinutes  = "d15/state/kwl/summerbypass/HystereseMinutes";
  constexpr const char* KwlHeatingAppCombUse       = "d15/state/kwl/heatingapp/combinedUse";

  constexpr const char* KwlDHT1Temperatur          = "d15/state/kwl/dht1/temperatur";
  constexpr const char* KwlDHT2Temperatur          = "d15/state/kwl/dht2/temperatur";
  constexpr const char* KwlDHT1Humidity            = "d15/state/kwl/dht1/humidity";
  constexpr const char* KwlDHT2Humidity            = "d15/state/kwl/dht2/humidity";
  constexpr const char* KwlCO2Abluft               = "d15/state/kwl/abluft/co2";
  constexpr const char* KwlVOCAbluft               = "d15/state/kwl/abluft/voc";


  // Die folgenden Topics sind nur für die SW-Entwicklung, und schalten Debugausgaben per mqtt ein und aus
  constexpr const char* KwlDebugsetFan1Getvalues   = "d15/debugset/kwl/fan1/getvalues";
  constexpr const char* KwlDebugsetFan2Getvalues   = "d15/debugset/kwl/fan2/getvalues";
  constexpr const char* KwlDebugstateFan1          = "d15/debugstate/kwl/fan1";
  constexpr const char* KwlDebugstateFan2          = "d15/debugstate/kwl/fan2";
  constexpr const char* KwlDebugstatePreheater     = "d15/debugstate/kwl/preheater";

  // Die folgenden Topics sind nur für die SW-Entwicklung, es werden Messwerte überschrieben, es kann damit der Sommer-Bypass und die Frostschutzschaltung getestet werden
  constexpr const char* KwlDebugsetTemperaturAussenluft = "d15/debugset/kwl/aussenluft/temperatur";
  constexpr const char* KwlDebugsetTemperaturZuluft     = "d15/debugset/kwl/zuluft/temperatur";
  constexpr const char* KwlDebugsetTemperaturAbluft     = "d15/debugset/kwl/abluft/temperatur";
  constexpr const char* KwlDebugsetTemperaturFortluft   = "d15/debugset/kwl/fortluft/temperatur";

  // Die folgenden Topics sind nur für die SW-Entwicklung, um Kalibrierung explizit zu setzen
  constexpr const char* KwlDebugsetFan1PWM         = "d15/debugset/kwl/fan1/pwm";
  constexpr const char* KwlDebugsetFan2PWM         = "d15/debugset/kwl/fan2/pwm";
  constexpr const char* KwlDebugsetFanPWMStore     = "d15/debugset/kwl/fan/pwm/store_IKNOWWHATIMDOING";
}
