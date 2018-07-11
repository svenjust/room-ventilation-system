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

#include "Task.h"

#include <Arduino.h>

namespace
{
  static const char SchedulerName[] PROGMEM = ("Scheduler");
  static const char AllTasksName[] PROGMEM = ("AllTasks");

  static unsigned long s_scheduler_current_time = 0;
  static bool s_is_in_loop = false;
  static TaskTimingStats s_scheduler_runtime_stats(reinterpret_cast<const __FlashStringHelper*>(&SchedulerName[0]));
  static TaskTimingStats s_total_runtime_stats(reinterpret_cast<const __FlashStringHelper*>(&AllTasksName[0]));
}

Subtask* Subtask::s_first_task_ = nullptr;

void TaskTimingStats::addRuntime(unsigned long runtime)
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

void TaskTimingStats::addPolltime(unsigned long polltime)
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

unsigned long TaskTimingStats::getAvgRuntime() const
{
  unsigned long count = (count_runtime_ - adjust_count_runtime_);
  if (count)
    return sum_runtime_ / count;
  else
    return 0;
}

unsigned long TaskTimingStats::getMaxRuntimeSinceStart() const
{
  return max_runtime_ > max_runtime_since_start_ ? max_runtime_ : max_runtime_since_start_;
}

unsigned long TaskTimingStats::getAvgPolltime() const {
  if (count_polltime_)
    return sum_polltime_ / count_polltime_;
  else
    return 0;
}

unsigned long TaskTimingStats::getMaxPolltimeSinceStart() const
{
  return max_polltime_ > max_polltime_since_start_ ? max_polltime_ : max_polltime_since_start_;
}

void TaskTimingStats::toString(char* buffer, unsigned size) const
{
  auto namelen = strlen_P(reinterpret_cast<const char*>(name_));
  if (namelen > size)
    namelen = size;
  memcpy_P(buffer, reinterpret_cast<const char*>(name_), namelen);
  if (namelen < size)
    snprintf(buffer + namelen, size - namelen,
      ": max=%lu/%lu, avg=%lu, cnt=%lu-%lu, pmax=%lu/%lu, pavg=%lu",
      max_runtime_, getMaxRuntimeSinceStart(), getAvgRuntime(), count_runtime_, adjust_count_runtime_,
      max_polltime_, getMaxPolltimeSinceStart(), getAvgPolltime());
}

void TaskTimingStats::resetMaximum()
{
  max_runtime_since_start_ = getMaxRuntimeSinceStart();
  max_polltime_since_start_ = getMaxPolltimeSinceStart();
  max_runtime_ = 0;
  max_polltime_ = 0;
}

Subtask::Subtask(TaskTimingStats& stats, void* instance, void (*invoker)(void*), void (*poller)(void*)) :
  stats_(stats),
  invoker_(invoker),
  poller_(poller),
  instance_ptr_(instance),
  next_(s_first_task_)
{
  s_first_task_ = this;
}

void Subtask::runRepeated(unsigned long timeout, unsigned long interval)
{
  unsigned long new_time;
  if (s_is_in_loop) {
    new_time = s_scheduler_current_time + timeout;
  } else {
    static unsigned long s_startup_delay = 0;
    new_time = micros() + timeout + s_startup_delay;
    s_startup_delay = (s_startup_delay + 223500) & 0xffffffUL;  // ~220ms apart at startup, max. 1s
  }
  if (new_time == next_time_)
    ++new_time;   // this is for the unlikely case where re-add inside run() will use exactly the same
                  // time as before, which would remove the task from scheduling
  if (!new_time)
    new_time = 1; // 0 is special for not scheduled
  next_time_ = new_time;
  interval_ = interval;
}

void Subtask::cancel()
{
  next_time_ = interval_ = 0;
}

void Subtask::invoke()
{
  invoker_(instance_ptr_);
}

bool Subtask::poll()
{
  if (poller_) {
    poller_(instance_ptr_);
    return true;
  } else {
    return false;
  }
}

void Task::loop()
{
  if (s_is_in_loop)
    return; // recursive call, ERROR

  s_is_in_loop = true;
  s_scheduler_current_time = micros();
  unsigned long schedule_start_time = s_scheduler_current_time;
  unsigned long all_task_times = 0;

  auto cur_task = Subtask::s_first_task_;
  while (cur_task) {
    auto task_time = cur_task->next_time_;
    auto next = cur_task->next_;
    unsigned long task_start_time = micros();
    bool had_poll = cur_task->poll();
    bool had_run = false;
    if (task_time) {
      long delta = long(task_time - s_scheduler_current_time);
      if (delta <= 0) {
        // OK, timer expired
        cur_task->invoke();
        auto interval = cur_task->interval_;
        if (interval) {
          // Interval task, compute next time to run the task. In case the next time would fall
          // into this loop run, skip one call. This protects against runaway tasks that are
          // scheduled too frequently.
          while (long(task_time - s_scheduler_current_time) < 0)
            task_time += interval;
          cur_task->next_time_ = task_time;
        } else {
          // Regular task, remove the task from the list. It has to be explicitly re-added.
          if (cur_task->next_time_ == task_time)
            cur_task->next_time_ = 0; // was not re-added in the meantime
        }
        had_run = true;
      }
    }
    if (had_poll || had_run) {
      auto task_runtime = micros() - task_start_time;
      if (had_run)
        cur_task->stats_.addRuntime(task_runtime);
      else
        cur_task->stats_.addPolltime(task_runtime);
      all_task_times += task_runtime;
    }
    cur_task = next;
  }

  if (all_task_times) {
    // at least one task was scheduled, record how long did scheduler take
    auto sched_runtime = micros() - schedule_start_time;
    s_scheduler_runtime_stats.addRuntime(sched_runtime - all_task_times);
    s_total_runtime_stats.addRuntime(sched_runtime);
  }
  s_is_in_loop = false;
}

Task::iterator Task::begin()
{
  return iterator(Subtask::s_first_task_);
}

Task::iterator Task::end()
{
  return iterator(nullptr);
}

TaskTimingStats& Task::getSchedulerStatistics()
{
  return s_scheduler_runtime_stats;
}

TaskTimingStats& Task::getAllTasksStatistics()
{
  return s_total_runtime_stats;
}

void Task::resetAllMaxima()
{
  for (auto i = begin(); i != end(); ++i)
    i->getStatistics().resetMaximum();
  s_scheduler_runtime_stats.resetMaximum();
  s_total_runtime_stats.resetMaximum();
}

Task::iterator& Task::iterator::operator++()
{
  cur_ = cur_->next_;
  find_next_valid();
  return *this;
}

Task::iterator::iterator(Subtask* task) :
  cur_(task)
{
  find_next_valid();
}

void Task::iterator::find_next_valid()
{
  // move to the next real task
  while (cur_ && &cur_->stats_ != &static_cast<Task*>(cur_)->real_stats_)
    cur_ = cur_->next_;
}
