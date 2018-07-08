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

#include "TimingStats.h"
#include "MessageHandler.h"
#include "MQTTTopic.hpp"

#include <stdio.h>

void TimingStats::addRuntime(unsigned long runtime)
{
  if (runtime > max_runtime_)
    max_runtime_ = runtime;
  auto sum = sum_runtime_ + runtime;
  if (sum < sum_runtime_) {
    // overflow on sum of runtimes, cut in half
    sum_runtime_ >>= 1;
    // compute # of "eaten" measurements
    adjust_count_runtime_ += (count_runtime_ - adjust_count_runtime_) >> 1;
  }
  sum_runtime_ += runtime;
  ++count_runtime_;
}

void TimingStats::addPolltime(unsigned long polltime)
{
  if (polltime > max_polltime_)
    max_polltime_ = polltime;
  auto sum = sum_polltime_ + polltime;
  if (sum < sum_polltime_) {
    // overflow on sum of polltimes, cut in half
    sum_polltime_ >>= 1;
    count_polltime_ >>= 1;
  }
  sum_polltime_ += polltime;
  if (++count_polltime_ == 0) {
    sum_polltime_ >>= 1;
    count_polltime_ = 0x8000U;
  }
}

unsigned long TimingStats::getAvgRuntime() const {
  unsigned long count = (count_runtime_ - adjust_count_runtime_);
  if (count)
    return sum_runtime_ / count;
  else
    return 0;
}

unsigned long TimingStats::getAvgPolltime() const {
  if (count_polltime_)
    return sum_polltime_ / count_polltime_;
  else
    return 0;
}

void TimingStats::sendStats(MessageHandler& handler, unsigned long next) const {
  char buffer[140]; // leaves at least 30B for task name
  char* dest = strcpy_P(buffer, reinterpret_cast<const char*>(name_)) + strlen(buffer);
  snprintf(dest, sizeof(buffer) - (dest - buffer),
      ": max=%lu, avg=%lu, cnt=%lu-%lu, next=%lu, pmax=%lu, pavg=%lu",
      max_runtime_, getAvgRuntime(), count_runtime_, adjust_count_runtime_, next,
      max_polltime_, getAvgPolltime());
  handler.publish(MQTTTopic::KwlDebugstateScheduler, buffer, false);
}
