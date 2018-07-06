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

unsigned long TimingStats::getAvgRuntime() const {
  unsigned long count = (count_runtime_ - adjust_count_runtime_);
  if (count)
    return sum_runtime_ / count;
  else
    return 0;
}

void TimingStats::sendStats(MessageHandler& handler, unsigned long next) const {
  char buffer[80];
  snprintf(buffer, sizeof(buffer), "%s: max=%lu, avg=%lu, cnt=%lu-%lu, next=%lu",
      name_, max_runtime_, getAvgRuntime(),
      count_runtime_, adjust_count_runtime_, next);
  handler.publish(MQTTTopic::KwlDebugstateScheduler, buffer, false);
}
