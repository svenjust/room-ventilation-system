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

#include "KWLConfig.h"

#include <IPAddress.h>

IPAddressLiteral::operator IPAddress() const
{
  return IPAddress(ip[0], ip[1], ip[2], ip[3]);
}

#define KWL_COPY(name) name##_ = KWLConfig::Standard##name

static_assert(sizeof(KWLPersistentConfig) == 240, "Persistent config size changed, ensure compatibility or increment version");

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
  if (crashes_[0].real_time == 0xffffffffUL) {
    Serial.println(F("Config migration: clearing crash reports"));
    memset(crashes_, 0, sizeof(crashes_));
    update(crashes_);
  }
}

void KWLPersistentConfig::storeCrash(uint32_t pc, unsigned sp, uint32_t real_time)
{
  auto runtime = millis();
  unsigned oldest_crash = 0;
  uint32_t oldest_time = real_time;
  uint32_t longest_runtime = 0;
  for (size_t i = 0; i < KWLConfig::MaxCrashReportCount; ++i) {
    if (crashes_[i].crash_addr == 0) {
      // unused slot, take it
      oldest_crash = i;
      break;
    } else if (real_time && crashes_[i].real_time < oldest_time) {
      oldest_crash = i;
      oldest_time = crashes_[i].real_time;
    } else if (crashes_[i].millis > longest_runtime) {
      oldest_crash = i;
      longest_runtime = crashes_[i].millis;
    }
  }
  auto& c = crashes_[oldest_crash];
  c.crash_sp = sp;
  c.crash_addr = pc;
  c.real_time = real_time;
  c.millis = runtime;
  update(c);
}

void KWLPersistentConfig::resetCrashes()
{
  memset(crashes_, 0, sizeof(crashes_));
  update(crashes_);
}
