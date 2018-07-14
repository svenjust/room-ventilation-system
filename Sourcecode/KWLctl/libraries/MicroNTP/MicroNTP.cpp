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

#include "MicroNTP.h"

#include <Udp.h>
#include <Arduino.h>

/// NTP port of the server.
static constexpr uint16_t NTP_PORT = 123;
/// Local port to use.
static constexpr uint16_t LOCAL_PORT = 50123;
/// Delta for conversion between NTP time and Unix epoch.
static constexpr uint32_t NTP_TIMESTAMP_DELTA = 2208988800UL;
/// Divider to use to convert fractional time to milliseconds.
static constexpr uint32_t NTP_FRACTION_DIVIDER = 4294968UL;
/// Interval for querying NTP time if it is unknown (every 5s).
static constexpr uint32_t NTP_INITIAL_QUERY_INTERVAL = 5UL*1000;
/// Re-query time every 5 minutes.
static constexpr uint32_t NTP_QUERY_INTERVAL = 5UL*60*1000;
/// Expire NTP information after 14 days of no new time.
static constexpr uint32_t NTP_INVALID_INTERVAL = 14UL*24*60*60*1000;

/// Flag for debugging on serial port.
static constexpr bool DEBUG = false;

/// NTP packet structure.
struct ntp_packet
{
  uint8_t li_vn_mode = 0x1b;   // Eight bits. li, vn, and mode.
                               // li.   Two bits.   Leap indicator.
                               // vn.   Three bits. Version number of the protocol.
                               // mode. Three bits. Client will pick mode 3 for client.

  uint8_t stratum = 0;         // Eight bits. Stratum level of the local clock.
  uint8_t poll = 0;            // Eight bits. Maximum interval between successive messages.
  uint8_t precision = 0;       // Eight bits. Precision of the local clock.

  uint32_t rootDelay = 0;      // 32 bits. Total round trip delay time.
  uint32_t rootDispersion = 0; // 32 bits. Max error aloud from primary clock source.
  uint32_t refId = 0;          // 32 bits. Reference clock identifier.

  uint32_t refTm_s = 0;        // 32 bits. Reference time-stamp seconds.
  uint32_t refTm_f = 0;        // 32 bits. Reference time-stamp fraction of a second.

  uint32_t origTm_s = 0;       // 32 bits. Originate time-stamp seconds.
  uint32_t origTm_f = 0;       // 32 bits. Originate time-stamp fraction of a second.

  uint32_t rxTm_s = 0;         // 32 bits. Received time-stamp seconds.
  uint32_t rxTm_f = 0;         // 32 bits. Received time-stamp fraction of a second.

  uint32_t txTm_s = 0;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
  uint32_t txTm_f = 0;         // 32 bits. Transmit time-stamp fraction of a second.

  // Total: 384 bits or 48 bytes.
};

uint32_t ntohl(uint32_t value) {
  uint32_t tmp_a = (value & 0xff000000) >> 24;
  uint32_t tmp_b = (value & 0x00ff0000) >> 8;
  uint32_t tmp_c = (value & 0x0000ff00) << 8 ;
  uint32_t tmp_d = (value & 0x000000ff) << 24;
  return tmp_a | tmp_b |tmp_c | tmp_d;
}

MicroNTP::MicroNTP(UDP& udp, IPAddress ip) :
  udp_(udp), ip_(ip)
{
}

void MicroNTP::begin()
{
  if (DEBUG)
    Serial.println(F("NTP: initializing"));
  udp_.begin(LOCAL_PORT);
}

void MicroNTP::loop()
{
  auto ms = millis();
  if (parseReply(ms))
    return;

  auto delta = ms - sent_time_ms_;
  if (delta >= NTP_INVALID_INTERVAL && current_ntp_time_) {
    if (DEBUG)
      Serial.println(F("NTP: time expired, no NTP reply since long time (2 weeks default)"));
    current_ntp_time_ = 0;
  }
  if (delta >= NTP_QUERY_INTERVAL) {
    if (DEBUG)
      Serial.println(F("NTP: query interval reached"));
    sendRequest(ms);
  } else if (!current_ntp_time_ && delta > NTP_INITIAL_QUERY_INTERVAL) {
    if (DEBUG)
      Serial.println(F("NTP: no time yet and initial query interval reached"));
    sendRequest(ms);
  }
}

void MicroNTP::forceQuery()
{
  if (DEBUG)
    Serial.println(F("NTP: forcing query"));
  sent_time_ms_ = millis() - NTP_QUERY_INTERVAL;
}

unsigned long MicroNTP::currentTime() const
{
  return time(millis());
}

unsigned long MicroNTP::time(unsigned long ms) const
{
  if (current_ntp_time_) {
    ms = ms + ntp_time_millis_fract_ - receive_time_ms_;
    return current_ntp_time_ + (ms / 1000);
  } else {
    return 0;
  }
};

bool MicroNTP::sendRequest(uint32_t ms)
{
  ntp_packet tmp;
  udp_.beginPacket(ip_, NTP_PORT);
  udp_.write(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
  bool res = udp_.endPacket() != 0;
  if (DEBUG) {
    if (res)
      Serial.print(F("NTP: sent packet at ms="));
    else
      Serial.print(F("NTP: failed to send packet at ms="));
    Serial.println(ms);
  }
  sent_time_ms_ = ms;
  return res;
}

bool MicroNTP::parseReply(uint32_t ms)
{
  int packetSize = udp_.parsePacket();
  if (packetSize > 0) {
    if (udp_.remotePort() != NTP_PORT) {
      if (DEBUG) {
        Serial.print(F("NTP: received packet from non-NTP port "));
        Serial.println(udp_.remotePort());
      }
      return false;
    }
    if (packetSize < int(sizeof(ntp_packet))) {
      if (DEBUG) {
        Serial.print(F("NTP: received too small NTP packet, size "));
        Serial.println(packetSize);
      }
      return false;
    }
    // assume NTP packet
    auto expected_time = currentTime();
    ntp_packet tmp;
    udp_.read(reinterpret_cast<uint8_t*>(&tmp), sizeof(ntp_packet));
    receive_time_ms_ = ms;
    current_ntp_time_ = ntohl(tmp.txTm_s) - NTP_TIMESTAMP_DELTA;
    ntp_time_millis_fract_ = ntohl(tmp.txTm_f) / NTP_FRACTION_DIVIDER;
    if (DEBUG) {
      Serial.print(F("NTP: got time "));
      Serial.print(current_ntp_time_);
      Serial.print('s');
      Serial.print('+');
      Serial.print(ntp_time_millis_fract_);
      Serial.print(F("ms, at Arduino ms="));
      Serial.print(ms);
      Serial.print(F(", UTC h:m:s/wd="));
      auto hms = timeHMS(ms, 0, false);
      Serial.print(PrintableHMS(hms));
      Serial.print('/');
      Serial.print(hms.wd);
      if (expected_time) {
        Serial.print(F(", delta="));
        Serial.print(long(currentTime() - expected_time));
      }
      Serial.println();
    }
    return true;
  }
  return false;
}
