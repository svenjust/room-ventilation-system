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

#include "KWLControl.hpp"
#include "KWLConfig.h"
#include "MQTTTopic.hpp"

#include <EthernetUdp.h>

// forwards
void loopDisplayUpdate();
void loopTouch();

KWLControl::KWLControl() :
  Task(F("KWLControl"), *this, &KWLControl::run, &KWLControl::poll),
  ntp_(udp_, KWLConfig::NetworkNTPServer),
  network_client_(persistent_config_, ntp_),
  fan_control_(persistent_config_, this),
  bypass_(persistent_config_, temp_sensors_),
  antifreeze_(fan_control_, temp_sensors_, persistent_config_),
  program_manager_(persistent_config_, fan_control_, ntp_),
  display_update_(F("DisplayUpdate"), *this, &KWLControl::dummy, &KWLControl::displayUpdate),
  process_touch_(F("ProcessTouch"), *this, &KWLControl::dummy, &KWLControl::processTouch)
{}

void KWLControl::begin(Print& initTracer)
{
  persistent_config_.begin(initTracer, KWLConfig::FACTORY_RESET_EEPROM);
  network_client_.begin(initTracer);
  temp_sensors_.begin(initTracer);
  fan_control_.begin(initTracer);
  bypass_.begin(initTracer);
  antifreeze_.begin(initTracer);
  add_sensors_.begin(initTracer);
  ntp_.begin();
  program_manager_.begin();

  // run error check loop every second, but give some time to initialize first
  runRepeated(8000000, 1000000);
}

void KWLControl::errorsToString(char* buffer, size_t size)
{
  /*
  buffer[0] = 0;
  if ((errors_ & (ERROR_BIT_FAN1 | ERROR_BIT_FAN2)) == (ERROR_BIT_FAN1 | ERROR_BIT_FAN2))
    strlcpy_P(buffer, PSTR("Beide Luefter ausgefallen"), size);
  else if (errors_ & ERROR_BIT_FAN1) {
    strlcpy_P(buffer, PSTR("Zuluftluefter ausgefallen"), size);
  }
  else if (errors_ & ERROR_BIT_FAN2) {
    strlcpy_P(buffer, PSTR("Abluftluefter ausgefallen"), size);
  }

  if (errors_ & (ERROR_BIT_T1 | ERROR_BIT_T2 | ERROR_BIT_T3 | ERROR_BIT_T4)) {
    if (errors_ & (ERROR_BIT_FAN1 | ERROR_BIT_FAN2))
      strlcat_P(buffer, PSTR("; "), size);
    strlcat_P(buffer, PSTR("Temperatursensor(en) ausgefallen:"), size);
    if (errors_ & ERROR_BIT_T1) strlcat_P(buffer, PSTR(" T1"), size);
    if (errors_ & ERROR_BIT_T2) strlcat_P(buffer, PSTR(" T2"), size);
    if (errors_ & ERROR_BIT_T3) strlcat_P(buffer, PSTR(" T3"), size);
    if (errors_ & ERROR_BIT_T4) strlcat_P(buffer, PSTR(" T4"), size);
  }
  */
  if (errors_ == 0) {
    buffer[0] = 0;
    return;
  }
  strlcpy_P(buffer, PSTR("Ausfall:"), size);
  if (errors_ & ERROR_BIT_FAN1) strlcat_P(buffer, PSTR(" Zuluftluefter"), size);
  if (errors_ & ERROR_BIT_FAN2) strlcat_P(buffer, PSTR(" Abluftluefter"), size);
  if (errors_ & ERROR_BIT_T1) strlcat_P(buffer, PSTR(" T1"), size);
  if (errors_ & ERROR_BIT_T2) strlcat_P(buffer, PSTR(" T2"), size);
  if (errors_ & ERROR_BIT_T3) strlcat_P(buffer, PSTR(" T3"), size);
  if (errors_ & ERROR_BIT_T4) strlcat_P(buffer, PSTR(" T4"), size);
  buffer[size - 1] = 0;
}

void KWLControl::infosToString(char* buffer, size_t size)
{
  unsigned value = info_ & INFO_VALUE_MASK;
  switch (info_ & INFO_TYPE_MASK) {
  case INFO_CALIBRATION:
  {
    char tmp[6];
    itoa(int(value), tmp, 10);
    strlcpy_P(buffer, PSTR("Luefter werden kalibriert fuer Stufe "), size);
    strlcat(buffer, tmp, size);
    strlcat_P(buffer, PSTR(". Bitte warten..."), size);
    break;
  }

  case INFO_PREHEATER:
  {
    char tmp[6];
    snprintf(tmp, sizeof(tmp), "%u%%", value);
    strlcpy_P(buffer, PSTR("Defroster: Vorheizregister eingeschaltet "), size);
    strlcat(buffer, tmp, size);
    break;
  }

  case INFO_ANTIFREEZE:
    if (value == 0)
      strlcpy_P(buffer, PSTR("Defroster: Zuluftventilator ausgeschaltet!"), size);
    else
      strlcpy_P(buffer, PSTR("Defroster: Zu- und Abluftventilator AUS! (KAMIN)"), size);
    break;

  case INFO_BYPASS:
    if (value == 0)
      strlcpy_P(buffer, PSTR("Sommer-Bypassklappe wird geschlossen."), size);
    else
      strlcpy_P(buffer, PSTR("Sommer-Bypassklappe wird geoeffnet."), size);
    break;

  default:
    buffer[0] = 0;
  }
}

void KWLControl::fanSpeedSet()
{
  // this callback is called after computing new PWM tech points
  // and before setting fan speed via PWM
  antifreeze_.doActionAntiFreezeState();
}

bool KWLControl::mqttReceiveMsg(const StringView& topic, const StringView& s)
{
  // Set Values
  if (topic == MQTTTopic::CmdResetAll) {
    if (s == F("YES"))   {
      Serial.println(F("Speicherbereich wird gelöscht"));
      getPersistentConfig().factoryReset();
      // Reboot
      Serial.println(F("Reboot"));
      delay (1000);
      asm volatile ("jmp 0");
    }
  } else if (topic == MQTTTopic::KwlDebugsetSchedulerResetvalues) {
    // reset maximum runtimes for all tasks
    Task::resetAllMaxima();
  // Get Commands
  } else if (topic == MQTTTopic::CmdGetvalues) {
    // Alle Values
    getTempSensors().forceSend();
    getAntifreeze().forceSend();
    getFanControl().forceSend();
    getBypass().forceSend();
    getAdditionalSensors().forceSend();
  } else if (topic == MQTTTopic::KwlDebugsetSchedulerGetvalues) {
    // send statistics for scheduler
    auto i = Task::begin();
    uint8_t bitmask = 3;
    scheduler_publish_.publish([i, bitmask]() mutable {
      char buffer[150];
      // first send all tasks
      while (i != Task::end()) {
        i->getStatistics().toString(buffer, sizeof(buffer));
        if (!publish(MQTTTopic::KwlDebugstateScheduler, buffer, false))
          return false;
        ++i;
      }
      // tasks sent, now send overall info
      Task::getSchedulerStatistics().toString(buffer, sizeof(buffer));
      if (!publish_if(bitmask, uint8_t(1), MQTTTopic::KwlDebugstateScheduler, buffer, false))
        return false;
      Task::getAllTasksStatistics().toString(buffer, sizeof(buffer));
      if (!publish_if(bitmask, uint8_t(2), MQTTTopic::KwlDebugstateScheduler, buffer, false))
        return false;
      return true;
    });
  } else {
    return false;
  }
  return true;
}

void KWLControl::run()
{
  // In dieser Funktion wird auf verschiedene Fehler getestet und Felherbitmap gesets.
  // Fehlertext wird auf das Display geschrieben.

  unsigned local_err = 0;
  if (KWLConfig::StandardKwlModeFactor[fan_control_.getVentilationMode()] > 0.01) {
    if (fan_control_.getFan1().getSpeed() < 10 && antifreeze_.getState() == AntifreezeState::OFF)
      local_err |= ERROR_BIT_FAN1;
    if (fan_control_.getFan2().getSpeed() < 10)
      local_err |= ERROR_BIT_FAN2;
  }
  if (temp_sensors_.get_t1_outside() <= TempSensors::INVALID)
    local_err |= ERROR_BIT_T1;
  if (temp_sensors_.get_t2_inlet() <= TempSensors::INVALID)
    local_err |= ERROR_BIT_T2;
  if (temp_sensors_.get_t3_outlet() <= TempSensors::INVALID)
    local_err |= ERROR_BIT_T3;
  if (temp_sensors_.get_t4_exhaust() <= TempSensors::INVALID)
    local_err |= ERROR_BIT_T4;

  unsigned local_info = 0;
  if (fan_control_.getMode() == FanMode::Calibration)
    local_info = INFO_CALIBRATION | unsigned(fan_control_.getVentilationCalibrationMode());
  else if (antifreeze_.getState() == AntifreezeState::PREHEATER)
    local_info = INFO_PREHEATER | unsigned(antifreeze_.getPreheaterState());
  else if (antifreeze_.getState() == AntifreezeState::FAN_OFF)
    local_info = INFO_ANTIFREEZE;
  else if (antifreeze_.getState() == AntifreezeState::FIREPLACE)
    local_info = INFO_ANTIFREEZE | 1;
  else if (bypass_.isRunning())
    local_info = INFO_BYPASS | ((bypass_.getTargetState() == SummerBypassFlapState::OPEN) ? 1 : 0);

  if (errors_ != local_err || info_ != local_info) {
    // publish status via MQTT
    error_publish_.publish([local_err, local_info]() {
      char buffer[11];
      snprintf(buffer, sizeof(buffer), "0x%04X%04X", local_err, local_info);
      return publish(MQTTTopic::StatusBits, buffer, KWLConfig::RetainStatusBits);
    });
    errors_ = local_err;
    info_ = local_info;
  }
}

void KWLControl::poll()
{
  ntp_.loop();
}

void KWLControl::displayUpdate() {
  loopDisplayUpdate();
}

void KWLControl::processTouch() {
  loopTouch();
}
