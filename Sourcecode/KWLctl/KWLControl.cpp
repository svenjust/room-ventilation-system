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
#include "ScreenshotService.h"

#include <EthernetUdp.h>
#include <Wire.h>
#include <DeadlockWatchdog.h>
#include <avr/wdt.h>

KWLControl::KWLControl() :
  MessageHandler(F("KWLControl")),
  ntp_(udp_),
  network_client_(persistent_config_, ntp_),
  fan_control_(persistent_config_, this),
  bypass_(persistent_config_, temp_sensors_),
  antifreeze_(fan_control_, temp_sensors_, persistent_config_),
  program_manager_(persistent_config_, fan_control_, ntp_),
  control_stats_(F("KWLControl")),
  control_timer_(control_stats_, &KWLControl::run, *this)
{}

void KWLControl::begin(Print& initTracer)
{
  if (KWLConfig::ControlFansDAC) {
    // TODO Also if using Preheater DAC, but no Fan DAC
    Wire.begin();               // I2C-Pins definieren
    initTracer.println(F("Initialisierung DAC"));
  }

  persistent_config_.begin(initTracer, KWLConfig::FACTORY_RESET_EEPROM);
  network_client_.begin(initTracer);
  temp_sensors_.begin(initTracer);
  fan_control_.begin(initTracer);
  bypass_.begin(initTracer);
  antifreeze_.begin(initTracer);
  add_sensors_.begin(initTracer);
  ntp_.begin(persistent_config_.getNetworkNTPServer());
  program_manager_.begin();

  // run error check loop every second, but give some time to initialize first
  control_timer_.runRepeated(8000000, 1000000);

  if (persistent_config_.hasCrash()) {
    initTracer.println(F("*** NOTE *** Crash reports recorded in EEPROM"));
    for (uint8_t i = 0; i < KWLConfig::MaxCrashReportCount; ++i) {
      auto& c = persistent_config_.getCrash(i);
      if (c.crash_addr) {
        Serial.print(F(" - PC "));
        Serial.print(c.crash_addr, HEX);
        Serial.print('/');
        Serial.print(c.crash_addr * 2, HEX);
        Serial.print(F(", sp "));
        Serial.print(c.crash_sp, HEX);
        Serial.print(F(", timestamp "));
        Serial.print(c.real_time);
        Serial.print(F(", millis "));
        Serial.println(c.millis);
      }
    }
    errors_ = ERROR_BIT_CRASH;
  }

  if (antifreeze_.getHeatingAppCombUse()) {
    initTracer.println(F("...System mit Feuerstaettenbetrieb"));
  }

  // Setup fertig
  initTracer.println(F("Setup completed..."));

  tft_.begin(initTracer, *this);

  DeadlockWatchdog::begin(&deadlockDetected, this);
}

void KWLControl::errorsToString(char* buffer, size_t size)
{
  if ((errors_ & ~(ERROR_BIT_CRASH | ERROR_BIT_NTP)) == 0) {
    // do not report crash report presence as error string
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

void KWLControl::loop()
{
  scheduler_.loop();
  DeadlockWatchdog::reset();
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
      Serial.flush();
      delay(100);
      wdt_disable();
      asm volatile ("jmp 0");
    }
  } else if (topic == MQTTTopic::CmdRestart) {
    if (s == F("YES"))   {
      // Reboot
      Serial.println(F("Reboot"));
      Serial.flush();
      delay(100);
      wdt_disable();
      asm volatile ("jmp 0");
    }
  } else if (topic == MQTTTopic::KwlDebugsetSchedulerResetvalues) {
    // reset maximum runtimes for all tasks
    for (auto i = Scheduler::TaskTimingStats::begin(); i != Scheduler::TaskTimingStats::end(); ++i)
      i->resetMaximum();
    for (auto i = Scheduler::TaskPollingStats::begin(); i != Scheduler::TaskPollingStats::end(); ++i)
      i->resetMaximum();
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
    auto i1 = Scheduler::TaskPollingStats::begin();
    auto i2 = Scheduler::TaskTimingStats::begin();
    scheduler_publish_.publish([i1, i2]() mutable {
      char buffer[80];
      char tbuffer[40];
      MQTTTopic::KwlDebugstateScheduler.store(tbuffer);
      char* p = tbuffer + MQTTTopic::KwlDebugstateScheduler.length();
      static constexpr size_t rsize = sizeof(tbuffer) - MQTTTopic::KwlDebugstateScheduler.length() - 1;
      while (i1 != Scheduler::TaskPollingStats::end()) {
        strncpy_P(p, reinterpret_cast<const char*>(i1->getName()), rsize);
        p[rsize] = 0;
        i1->toString(buffer, sizeof(buffer));
        if (publish(tbuffer, buffer, false))
          ++i1;
        return false;
      }
      while (i2 != Scheduler::TaskTimingStats::end()) {
        strncpy_P(p, reinterpret_cast<const char*>(i2->getName()), rsize);
        p[rsize] = 0;
        i2->toString(buffer, sizeof(buffer));
        if (publish(tbuffer, buffer, false))
          ++i2;
        return false;
      }
      return true;
    });
  } else if (topic == MQTTTopic::KwlDebugsetNTPTime) {
    // set NTP time
    unsigned long time = static_cast<unsigned long>(s.toInt());
    ntp_.debugSetTime(time);
    if (KWLConfig::serialDebug) {
      Serial.print(F("Setting NTP time to "));
      Serial.print(time);
      Serial.print(F(", "));
      Serial.println(PrintableHMS(ntp_.currentTimeHMS(persistent_config_.getTimezoneMin() * 60, persistent_config_.getDST())));
    }
  } else if (topic == MQTTTopic::KwlDebugsetCrashGetvalues) {
    // get crash information
    unsigned index = 0;
    scheduler_publish_.publish([this, index]() mutable {
      while (index < KWLConfig::MaxCrashReportCount) {
        auto& c = persistent_config_.getCrash(index);
        if (c.crash_addr) {
          char buffer[48], topic[MQTTTopic::KwlDebugstateCrash.length() + 3];
          MQTTTopic::KwlDebugstateCrash.store(topic);
          char* p = topic + MQTTTopic::KwlDebugstateCrash.length();
          *p++ = char(index / 10) + '0';
          *p++ = (index % 10) + '0';
          *p = 0;
          snprintf_P(buffer, sizeof(buffer), PSTR("ip %06lx sp %03lx ntp %lu ms %lu"),
                   c.crash_addr * 2, c.crash_sp, c.real_time, c.millis);
          if (MessageHandler::publish(topic, buffer))
            ++index;
          return false;
        }
        ++index;
      }
      return true;
    });
  } else if (topic == MQTTTopic::KwlDebugsetCrashResetvalues) {
    // reset crash information
    persistent_config_.resetCrashes();
    errors_ &= ~ERROR_BIT_CRASH;
    mqttSendStatus();
  } else if (topic == MQTTTopic::KwlDebugsetCrashProvoke) {
    if (s == F("YES"))   {
      // provoke a crash by making a deadlock
      Serial.println(F("CRASH: Deadlock provoked"));
      Serial.flush();
      while (true) {}
    }
  } else if (topic == MQTTTopic::CmdScreenshot) {
    IPAddress ip;
    uint16_t port = 4444;
    {
      auto ip_str = s.c_str();
      auto port_str = strchr(ip_str, ':');
      if (port_str) {
        *const_cast<char*>(port_str++) = 0;
        port = uint16_t(atoi(port_str));
      }
      if (!ip.fromString(ip_str)) {
        Serial.println(F("Screenshot: invalid IP address"));
        return true;
      }
      if (!port) {
        Serial.println(F("Screenshot: invalid port specified"));
        return true;
      }
    }
    if (KWLConfig::serialDebug) {
      Serial.print(F("Screenshot: trigger for "));
      Serial.print(ip);
      Serial.print(':');
      Serial.print(port);
      Serial.print(F(" received at "));
      Serial.println(millis());
    }
    tft_.prepareForScreenshot();
    EthernetClient client;
    if (!client.connect(ip, port)) {
      if (KWLConfig::serialDebug) {
        Serial.println(F("Screenshot: cannot connect"));
        return true;
      }
    }
    if (KWLConfig::serialDebug)
      Serial.println(F("Screenshot: connected"));
    ScreenshotService::make(tft_.getTFT(), client);
    client.flush();
    client.stop();
    if (KWLConfig::serialDebug) {
      Serial.print(F("Screenshot: done at "));
      Serial.println(millis());
    }
  } else if (topic == MQTTTopic::CmdScreen) {
    // switch to given screen by ID
    tft_.gotoScreen(s.toInt());
  } else if (topic == MQTTTopic::CmdTouch) {
    // simulate touch at x,y
    int x, y;
    if (sscanf_P(s.c_str(), PSTR("%d,%d"), &x, &y) == 2)
      tft_.makeTouch(x, y);
  } else {
    return false;
  }
  return true;
}

void KWLControl::run()
{
  // In dieser Funktion wird auf verschiedene Fehler getestet und Felherbitmap gesets.
  // Fehlertext wird auf das Display geschrieben.

  unsigned local_err = errors_ & ERROR_BIT_CRASH;
  if (KWLConfig::StandardKwlModeFactor[fan_control_.getVentilationMode()] > 0.01) {
    if (fan_control_.getFan1().getSpeed() < 10 && antifreeze_.getState() == AntifreezeState::OFF)
      local_err |= ERROR_BIT_FAN1;
    if (fan_control_.getFan2().getSpeed() < 10)
      local_err |= ERROR_BIT_FAN2;
  }
  if (!ntp_.hasTime())
    local_err |= ERROR_BIT_NTP;
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
    errors_ = local_err;
    info_ = local_info;
    mqttSendStatus();
  }
}

void KWLControl::mqttSendStatus()
{
  error_publish_.publish([this]() {
    char buffer[11];
    snprintf_P(buffer, sizeof(buffer), PSTR("0x%04X%04X"), errors_, info_);
    return publish(MQTTTopic::StatusBits, buffer, KWLConfig::RetainStatusBits);
  });
}

void KWLControl::deadlockDetected(unsigned long pc, unsigned sp, void* arg)
{
  auto instance = reinterpret_cast<KWLControl*>(arg);
  instance->persistent_config_.storeCrash(pc, sp, instance->ntp_.currentTime());
}
