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

/*!
 * @file
 * @brief Minimalistic NTP (network time protocol) client.
 */
#pragma once

#include <IPAddress.h>

class UDP;

/*!
 * @brief Minimalistic NTP (network time protocol) client.
 *
 * This client + Ethernet UDP instance take only 60B of RAM.
 */
class MicroNTP
{
public:
  MicroNTP(const MicroNTP&) = delete;
  MicroNTP& operator=(const MicroNTP&) = delete;

  /// Parsed time.
  struct HMS {
    /// Hours (0-23).
    uint8_t h;
    /// Minutes (0-59).
    uint8_t m;
    /// Seconds (0-59).
    uint8_t s;
    /// Weekday (0-6, 0 == Monday).
    uint8_t wd;
  };

  /*!
   * @brief Construct NTP client.
   *
   * @param udp UDP protocol backend.
   * @param ip IP address of the NTP server.
   */
  MicroNTP(UDP& udp, IPAddress ip);

  /// Start communicating with NTP server.
  void begin();

  /// Call in loop to update time.
  void loop();

  /// Force sending query on next loop() call.
  void forceQuery();

  /*!
   * @brief Get current time in seconds since epoch.
   *
   * @return time in seconds or 0 if NTP not available.
   */
  unsigned long currentTime();

  /*!
   * @brief Get current time parsed into hours/minutes/seconds + weekday.
   *
   * This method allows you to get the time "cooked" without any expensive
   * libraries, if you just need to get the time itself + optionally a weekday.
   *
   * @param tz_offset timezone offset in seconds (e.g., 3600 for GMT+1).
   * @param dst daylight saving time flag (summer time; adds 3600 to timezeone offset).
   * @return time parsed into hours/minutes/seconds + weekday.
   */
  HMS currentTimeHMS(unsigned tz_offset, bool dst);

private:
  /// Send NTP request. Returns true, if packet was sent.
  bool sendRequest(uint32_t ms);
  /// Parse any outstanding UDP packet and return true, if NTP time received.
  bool parseReply(uint32_t ms);

  UDP& udp_;                            ///< UDP stream.
  IPAddress ip_;                        ///< IP address of the NTP server.
  unsigned long sent_time_ms_ = 0;      ///< Time at which last packet was sent.
  unsigned long receive_time_ms_ = 0;   ///< Millis() timer at the time packet came in.
  unsigned long current_ntp_time_ = 0;  ///< NTP time from NTP packet.
  unsigned int ntp_time_millis_fract_;  ///< NTP time fraction in milliseconds.
};
