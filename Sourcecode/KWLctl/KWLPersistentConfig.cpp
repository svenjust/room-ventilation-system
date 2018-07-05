/*
 * Copyright (C) 2018 Ivan Schréter (schreter@gmx.net)
 * Copyright (C) 2018 Sven Just (sven@familie-just.de)
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

#include "KWLPersistentConfig.h"
#include "kwl_config.h"

#define KWL_COPY(name) name##_ = kwl_config::Standard##name

void KWLPersistentConfig::loadDefaults()
{
  KWL_COPY(SpeedSetpointFan1);
  KWL_COPY(SpeedSetpointFan2);
  KWL_COPY(BypassTempAbluftMin);
  KWL_COPY(BypassTempAussenluftMin);
  KWL_COPY(BypassHystereseMinutes);
  KWL_COPY(BypassHystereseTemp);
  KWL_COPY(BypassManualSetpoint);
  KWL_COPY(BypassMode);

  UnusedFiller_ = 0;

  if (kwl_config::StandardModeCnt > 10) {
    Serial.println(F("ERROR: StandardModeCnt too big, max. 10 supported"));
  }
  for (unsigned i = 0; ((i < kwl_config::StandardModeCnt) && (i < 10)); i++) {
    FanPWMSetpoint_[i][0] = unsigned(kwl_config::StandardSpeedSetpointFan1 * kwl_config::StandardKwlModeFactor[i] * 1000 / kwl_config::StandardNenndrehzahlFan);
    FanPWMSetpoint_[i][1] = unsigned(kwl_config::StandardSpeedSetpointFan2 * kwl_config::StandardKwlModeFactor[i] * 1000 / kwl_config::StandardNenndrehzahlFan);
  }

  KWL_COPY(HeatingAppCombUse);
}
