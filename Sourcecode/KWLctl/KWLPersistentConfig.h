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

/*!
 * @file
 * @brief Persistent configuration of the ventilation system.
 */
#pragma once

#include "PersistentConfiguration.h"

#include <stdint.h>

/// EEPROM configuration version to expect/write.
static constexpr unsigned KWL_EEPROM_VERSION = 49;

// Helper for getters and setters.
#define KWL_GETSET(name) \
  decltype(name##_) get##name() const { return name##_; } \
  void set##name(decltype(name##_) value) { name##_ = value; update(name##_); }

/*!
 * @brief Persistent configuration of the ventilation system.
 */
class KWLPersistentConfig : public PersistentConfiguration<KWLPersistentConfig, KWL_EEPROM_VERSION>
{
private:
  friend class PersistentConfiguration<KWLPersistentConfig, KWL_EEPROM_VERSION>;

  // documentation see KWLConfig

  // NOTE: this is PERSISTENT layout, do not reorder unless you increase the version number
  unsigned SpeedSetpointFan1_;        // 2
  unsigned SpeedSetpointFan2_;        // 4
  unsigned BypassTempAbluftMin_;      // 6
  unsigned BypassTempAussenluftMin_;  // 8
  unsigned BypassHystereseMinutes_;   // 10
  unsigned BypassHystereseTemp_;      // 12
  unsigned BypassManualSetpoint_;     // 14
  unsigned BypassMode_;               // 16
  bool DST_;                          // 18
  uint8_t UnusedFiller_;              // 19
  int FanPWMSetpoint_[10][2];         // 20-59
  unsigned HeatingAppCombUse_;        // 60
  int16_t TimezoneMin_;               // 62

  /// Initialize with defaults, if version doesn't fit.
  void loadDefaults();

public:
  // default getters/setters
  KWL_GETSET(SpeedSetpointFan1)
  KWL_GETSET(SpeedSetpointFan2)
  KWL_GETSET(BypassTempAbluftMin)
  KWL_GETSET(BypassTempAussenluftMin)
  KWL_GETSET(BypassHystereseMinutes)
  KWL_GETSET(BypassHystereseTemp)
  KWL_GETSET(BypassManualSetpoint)
  KWL_GETSET(BypassMode)
  KWL_GETSET(DST)
  KWL_GETSET(HeatingAppCombUse)
  KWL_GETSET(TimezoneMin)

  int getFanPWMSetpoint(unsigned fan, unsigned idx) { return FanPWMSetpoint_[idx][fan]; }
  void setFanPWMSetpoint(unsigned fan, unsigned idx, int pwm) { FanPWMSetpoint_[idx][fan] = pwm; update(FanPWMSetpoint_[idx][fan]); }
};

#undef KWL_GETSET
