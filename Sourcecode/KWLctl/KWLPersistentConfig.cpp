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
#include "KWLConfig.h"

#define KWL_COPY(name) name##_ = KWLConfig::Standard##name

static_assert(sizeof(KWLPersistentConfig) == 192, "Persistent config size changed, ensure compatibility or increment version");

void KWLPersistentConfig::loadDefaults()
{
  KWL_COPY(SpeedSetpointFan1);
  KWL_COPY(SpeedSetpointFan2);
  KWL_COPY(BypassTempAbluftMin);
  KWL_COPY(BypassTempAussenluftMin);
  KWL_COPY(BypassHystereseMinutes);
  KWL_COPY(BypassHysteresisTemp);
  KWL_COPY(BypassManualSetpoint);
  KWL_COPY(DST);
  KWL_COPY(BypassMode);
  KWL_COPY(AntifreezeHystereseTemp);
  KWL_COPY(TimezoneMin);

  if (KWLConfig::StandardModeCnt > 10) {
    Serial.println(F("ERROR: StandardModeCnt too big, max. 10 supported"));
  }
  for (unsigned i = 0; ((i < KWLConfig::StandardModeCnt) && (i < 10)); i++) {
    FanPWMSetpoint_[i][0] = int(KWLConfig::StandardSpeedSetpointFan1 * KWLConfig::StandardKwlModeFactor[i] * 1000 / KWLConfig::StandardNenndrehzahlFan);
    FanPWMSetpoint_[i][1] = int(KWLConfig::StandardSpeedSetpointFan2 * KWLConfig::StandardKwlModeFactor[i] * 1000 / KWLConfig::StandardNenndrehzahlFan);
  }

  KWL_COPY(HeatingAppCombUse);

  memset(&programs_, 0, sizeof(programs_));
}

void KWLPersistentConfig::migrate()
{
  // "upgrade" existing config, if possible (all initialized to -1/0xff)
  if (TimezoneMin_ == -1) {
    Serial.println(F("Config migration: setting timezone"));
    TimezoneMin_ = KWLConfig::StandardTimezoneMin;
    update(TimezoneMin_);
  }
  if (*reinterpret_cast<const uint8_t*>(&DST_) == 0xff) {
    Serial.println(F("Config migration: setting DST"));
    DST_ = KWLConfig::StandardDST;
    update(DST_);
  }
  if (BypassHysteresisTemp_ == 0xff) {
    Serial.println(F("Config migration: setting bypass hysteresis temperature"));
    BypassHysteresisTemp_ = KWLConfig::StandardBypassHysteresisTemp;
    update(BypassHysteresisTemp_);
  }
  if (programs_[0].start_h_ == 0xff) {
    Serial.println(F("Config migration: clearing programs"));
    memset(programs_, 0, sizeof(programs_));
    update(programs_);
  }
}
