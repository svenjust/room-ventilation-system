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

#include "ProgramManager.h"
#include "KWLConfig.h"
#include "FanControl.h"
#include "MQTTTopic.hpp"
#include "StringView.h"

#include <MicroNTP.h>

/// Check current program every 5s.
static constexpr unsigned long PROGRAM_INTERVAL = 5000000;

ProgramManager::ProgramManager(KWLPersistentConfig& config, FanControl& fan, const MicroNTP& ntp) :
  MessageHandler(F("ProgramManager")),
  config_(config),
  fan_(fan),
  ntp_(ntp),
  stats_(F("ProgramManager")),
  timer_task_(stats_, &ProgramManager::run, *this)
{}

void ProgramManager::begin()
{
  timer_task_.runRepeated(PROGRAM_INTERVAL);
}

const ProgramData& ProgramManager::getProgram(unsigned index)
{
  return config_.getProgram(index);
}

void ProgramManager::setProgram(unsigned index, const ProgramData& program)
{
  if (index > KWLConfig::MaxProgramCount)
    return; // ERROR
  config_.setProgram(index, program);
  if (index == unsigned(current_program_))
    current_program_ = -1;
}

void ProgramManager::run()
{
  // TODO handle additional input, like humidity sensor

  if (!ntp_.hasTime()) {
    if (KWLConfig::serialDebugProgram)
      Serial.println(F("PROG: check - no time"));
    return;
  }
  auto time = ntp_.currentTimeHMS(config_.getTimezoneMin() * 60L, config_.getDST());
  if (KWLConfig::serialDebugProgram) {
    Serial.print(F("PROG: check at "));
    Serial.println(PrintableHMS(time));
  }

  // iterate all programs and pick one which hits
  int8_t program = -1;
  for (int8_t i = 0; i < KWLConfig::MaxProgramCount; ++i) {
    if (config_.getProgram(unsigned(i)).matches(time)) {
      program = i;
      break;
    }
  }
  if (program != current_program_) {
    // switch programs
    if (KWLConfig::serialDebugProgram) {
      Serial.print(F("PROG: new program "));
      Serial.print(program);
      Serial.print(F(" active, previous program "));
      Serial.print(current_program_);
      Serial.print(F(", set mode="));
    }
    if (program >= 0) {
      auto& prog = config_.getProgram(unsigned(program));
      if (KWLConfig::serialDebugProgram)
        Serial.println(prog.fan_mode_);
      fan_.setVentilationMode(prog.fan_mode_);
    } else {
      // TODO what default to use if no program set? It should be last user
      // settings outside of the program. Or default. For now, set default.
      if (KWLConfig::serialDebugProgram)
        Serial.println(KWLConfig::StandardKwlMode);
      fan_.setVentilationMode(KWLConfig::StandardKwlMode);
    }
    current_program_ = program;
  }
}

bool ProgramData::matches(HMS hms) const
{
  if (!enabled())
    return false; // inactive program

  HMS start(start_h_, start_m_);
  HMS end(end_h_, end_m_);
  uint8_t wd_bit = uint8_t(1 << hms.wd);
  if (start.compareTime(end) > 0) {
    // program crossing midnight
    if (hms.compareTime(start) >= 0) {
      // day 1
      if ((weekdays_ & wd_bit) == 0)
        return false; // wrong day
    } else if (hms.compareTime(end) < 0) {
      // day 2
      wd_bit <<= 1;
      if (wd_bit == VALID_FLAG)
        wd_bit = 1;
      if ((weekdays_ & wd_bit) == 0)
        return false; // wrong day
    } else {
      return false; // out of range
    }
  } else {
    // normal program
    if (hms.compareTime(start) < 0 || hms.compareTime(end) >= 0)
      return false; // out of range
    if ((weekdays_ & wd_bit) == 0)
      return false; // wrong day
  }
  // program matches
  return true;
}

bool ProgramManager::mqttReceiveMsg(const StringView& topic, const StringView& s)
{
  if (topic == MQTTTopic::CmdGetProgram) {
    // get specific program or ALL
    if (s == F("all")) {
      // send all programs
      unsigned i = 0;
      publisher_.publish([this, i]() mutable {
        while (i < KWLConfig::MaxProgramCount) {
          if (!mqttSendProgram(i))
            return false; // continue next time
          ++i;
        }
        return true;  // all sent
      });
    } else {
      unsigned i = unsigned(s.toInt());
      publisher_.publish([this, i]() { return mqttSendProgram(i); });
    }
  } else if (topic == MQTTTopic::CmdSetProgram) {
    // set specific program
    // Parse program string "## F HH:MM HH:MM M xxxxxxx", where ## is slot number,
    // F is flag 0 or 1 whether it's enabled, M is fan mode and xxxxxxx are flags for
    // weekdays indicating whether to run program on a given weekday (0 or 1).
    // Weekday flags are optional, if not set, run every day.
    unsigned slot, enabled, start_h, start_m, end_h, end_m, mode;
    char wd_buf[8];
    int rc = sscanf(s.c_str(), "%u %u %u:%u %u:%u %u %7s",
                    &slot, &enabled, &start_h, &start_m, &end_h, &end_m, &mode, wd_buf);
    if (rc < 7) {
      if (KWLConfig::serialDebugProgram) {
        Serial.print(F("PROG: Invalid program string, parsed items "));
        Serial.print(rc);
        Serial.print('/');
        Serial.println('8');
      }
      return true;
    }
    if (slot >= KWLConfig::MaxProgramCount) {
      if (KWLConfig::serialDebugProgram)
        Serial.println(F("PROG: Invalid program slot number"));
      return true;
    }
    ProgramData prog;
    if (start_h > 23 || start_m > 59) {
      if (KWLConfig::serialDebugProgram)
        Serial.println(F("PROG: Invalid start time"));
      return true;
    }
    prog.start_h_ = uint8_t(start_h);
    prog.start_m_ = uint8_t(start_m);
    if (end_h > 23 || end_m > 59) {
      if (KWLConfig::serialDebugProgram)
        Serial.println(F("PROG: Invalid end time"));
      return true;
    }
    prog.end_h_ = uint8_t(end_h);
    prog.end_m_ = uint8_t(end_m);
    if (mode >= KWLConfig::StandardModeCnt) {
      if (KWLConfig::serialDebugProgram)
        Serial.println(F("PROG: Invalid mode"));
      return true;
    }
    prog.fan_mode_ = uint8_t(mode);
    if (rc >= 8) {
      prog.weekdays_ = 0;
      const char* p = wd_buf;
      for (uint8_t bit = 1; bit < ProgramData::VALID_FLAG; bit <<= 1) {
        switch (*p++) {
        case '0':
          break;
        case '1':
          prog.weekdays_ |= bit;
          break;
        default:
          // invalid string
          if (KWLConfig::serialDebugProgram)
            Serial.println(F("PROG: Weekdays must be [01]{7}"));
          return true;
        }
      }
    } else {
      // all weekdays
      prog.weekdays_ = 0x7f;
    }
    if (enabled)
      prog.weekdays_ |= ProgramData::VALID_FLAG;
    prog.reserved_ = 0;
    setProgram(slot, prog);
  } else {
    return false;
  }
  return true;
}

bool ProgramManager::mqttSendProgram(unsigned index)
{
  if (index >= KWLConfig::MaxProgramCount)
    return true;
  auto& prog = config_.getProgram(index);
  // Build program string "## F HH:MM HH:MM M xxxxxxx", where ## is slot number,
  // F is flag 0 or 1 whether it's enabled, M is fan mode and xxxxxxx are flags
  // for weekdays indicating whether to run program on a given weekday (0 or 1).
  char buffer[27];
  buffer[0] = char((index / 10)) + '0';
  buffer[1] = char(index % 10) + '0';
  buffer[2] = ' ';
  buffer[3] = prog.enabled() ? '1' : '0';
  buffer[4] = ' ';
  HMS(prog.start_h_, prog.start_m_).writeHM(buffer + 5);
  buffer[10] = ' ';
  HMS(prog.end_h_, prog.end_m_).writeHM(buffer + 11);
  buffer[16] = ' ';
  buffer[17] = char(prog.fan_mode_ + '0');
  buffer[18] = ' ';
  char* p = buffer + 19;
  for (uint8_t bit = 1; bit < ProgramData::VALID_FLAG; bit <<= 1)
    *p++ = (prog.weekdays_ & bit) ? '1' : '0';
  *p = 0;
  return publish(MQTTTopic::KwlProgram, buffer, false);
}

