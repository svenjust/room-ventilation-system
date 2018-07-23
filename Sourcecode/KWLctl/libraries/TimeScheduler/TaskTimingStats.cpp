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

#include "TaskTimingStats.h"

#include <avr/pgmspace.h>
#include <stdio.h>

using namespace Scheduler;

TaskTimingStats* TaskTimingStats::s_first_stat_ = nullptr;

TaskTimingStats::TaskTimingStats(const __FlashStringHelper* name) noexcept :
  name_(name),
  next_(s_first_stat_)
{
  s_first_stat_ = this;
}

void TaskTimingStats::addRuntime(unsigned long runtime) noexcept
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

unsigned long TaskTimingStats::getAvgRuntime() const noexcept
{
  unsigned long count = (count_runtime_ - adjust_count_runtime_);
  if (count)
    return sum_runtime_ / count;
  else
    return 0;
}

unsigned long TaskTimingStats::getMaxRuntimeSinceStart() const noexcept
{
  return max_runtime_ > max_runtime_since_start_ ? max_runtime_ : max_runtime_since_start_;
}

void TaskTimingStats::toString(char* buffer, unsigned size) const noexcept
{
  auto FORMAT = PSTR("max %lu smax %lu avg %lu cnt %lu adj %lu");
  snprintf_P(buffer, size, FORMAT,
    max_runtime_, getMaxRuntimeSinceStart(), getAvgRuntime(),
    count_runtime_, adjust_count_runtime_);
}

void TaskTimingStats::resetMaximum() noexcept
{
  max_runtime_since_start_ = getMaxRuntimeSinceStart();
  max_runtime_ = 0;
}

TaskPollingStats* TaskPollingStats::s_first_stat_ = nullptr;

TaskPollingStats::TaskPollingStats(const __FlashStringHelper* name) noexcept :
  name_(name),
  next_(s_first_stat_)
{
  s_first_stat_ = this;
}

void TaskPollingStats::addPolltime(unsigned long polltime) noexcept
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

unsigned long TaskPollingStats::getAvgPolltime() const noexcept {
  if (count_polltime_)
    return sum_polltime_ / count_polltime_;
  else
    return 0;
}

unsigned long TaskPollingStats::getMaxPolltimeSinceStart() const noexcept
{
  return max_polltime_ > max_polltime_since_start_ ? max_polltime_ : max_polltime_since_start_;
}

void TaskPollingStats::toString(char* buffer, unsigned size) const noexcept
{
  auto FORMAT = PSTR("pmax %lu spmax %lu pavg %lu");
  snprintf_P(buffer, size, FORMAT,
    max_polltime_, getMaxPolltimeSinceStart(), getAvgPolltime());
}

void TaskPollingStats::resetMaximum() noexcept
{
  max_polltime_since_start_ = getMaxPolltimeSinceStart();
  max_polltime_ = 0;
}
