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

#include "TimeSchedulerHelpers.h"
#include "TimeScheduler.h"

#include <avr/pgmspace.h>

namespace
{
  static const char SchedulerName[] PROGMEM = ("Scheduler");
  static const char AllTasksName[] PROGMEM = ("AllTasks");

  static Scheduler::TaskTimingStats s_scheduler_runtime_stats(reinterpret_cast<const __FlashStringHelper*>(&SchedulerName[0]));
  static Scheduler::TaskTimingStats s_total_runtime_stats(reinterpret_cast<const __FlashStringHelper*>(&AllTasksName[0]));
}

unsigned long Scheduler::TimeScheduler::runTimedTasks() noexcept
{
  unsigned long all_task_times = 0;

  {
    auto cur_task = TimedTaskBase::s_first_task_;
    while (cur_task) {
      auto task_time = cur_task->next_time_;
      auto next = cur_task->next_;
      if (task_time) {
        unsigned long task_start_time = micros();
        long delta = long(task_time - TaskBase::s_scheduler_current_time_);
        if (delta <= 0) {
          // OK, timer expired
          auto end_time = cur_task->invoke(task_start_time);
          if (cur_task->next_time_ == task_time) {
            // task didn't reschedule itself
            auto interval = cur_task->interval_;
            if (interval) {
              // Interval task, compute next time to run the task. In case the next time would fall
              // into this loop run, skip one call. This protects against runaway tasks that are
              // scheduled too frequently.
              task_time += interval;
              delta = long(task_time - TaskBase::s_scheduler_current_time_);
              if (delta < 0) {
                // task must be skipped, compute next time
                task_time += ((static_cast<unsigned long>(-delta) / interval) + 1) * interval;
              }
              cur_task->next_time_ = task_time;
            } else {
              // Regular task, remove the task from the list.
              cur_task->next_time_ = 0;
            }
          }
          auto task_runtime = end_time - task_start_time;
          all_task_times += task_runtime;
        }
      }
      cur_task = next;
    }
  }

  return all_task_times;
}

void Scheduler::TimeScheduler::checkDeepSleep() noexcept
{
  if (!deep_sleep_)
    return;

  // TODO run over all tasks and collect next time
  auto cur = TimedTaskBase::s_first_task_;
  auto current_time = micros();
  unsigned long min = 1UL << 31;
  while (cur) {
    auto delta = cur->next_time_ - current_time;
    if (long(delta) < 1000)
      return; // less than 1ms to sleep - no point
    if (delta < min)
      min = delta;
    cur = cur->next_;
  }
  deep_sleep_(min);
}

void Scheduler::TimeScheduler::loop() noexcept
{
  if (TaskBase::s_is_in_loop_)
    return; // recursive call, ERROR

  TaskBase::s_is_in_loop_ = true;

  TaskBase::s_scheduler_current_time_ = micros();
  unsigned long schedule_start_time = TaskBase::s_scheduler_current_time_;
  auto all_task_times = runTimedTasks();

  if (all_task_times) {
    // at least one task was scheduled, record how long did scheduler take
    auto sched_runtime = micros() - schedule_start_time;
    s_scheduler_runtime_stats.addRuntime(sched_runtime - all_task_times);
    s_total_runtime_stats.addRuntime(sched_runtime);
  }

  checkDeepSleep();

  TaskBase::s_is_in_loop_ = false;
}

unsigned long Scheduler::PollingScheduler::runPollTasks() noexcept
{
  unsigned long all_task_times = 0;
  {
    auto cur_task = PollTaskBase::s_first_task_;
    auto task_start_time = micros();
    const auto poll_start_time = task_start_time;
    while (cur_task) {
      if (cur_task->isEnabled()) {
        auto task_end_time = cur_task->invoke(task_start_time);
        task_start_time = task_end_time;
      }
      cur_task = cur_task->next_;
    }
    all_task_times += task_start_time - poll_start_time;
  }
  return all_task_times;
}

void Scheduler::PollingScheduler::loop() noexcept
{
  if (TaskBase::s_is_in_loop_)
    return; // recursive call, ERROR

  TaskBase::s_is_in_loop_ = true;

  TaskBase::s_scheduler_current_time_ = micros();
  unsigned long schedule_start_time = TaskBase::s_scheduler_current_time_;
  auto all_task_times = runTimedTasks() + runPollTasks();

  if (all_task_times) {
    // at least one task was scheduled, record how long did scheduler take
    auto sched_runtime = micros() - schedule_start_time;
    s_scheduler_runtime_stats.addRuntime(sched_runtime - all_task_times);
    s_total_runtime_stats.addRuntime(sched_runtime);
  }

  checkDeepSleep();

  TaskBase::s_is_in_loop_ = false;
}

void Scheduler::PollingScheduler::checkDeepSleep() noexcept
{
  if (deep_sleep_) {
    auto cur = PollTaskBase::s_first_task_;
    while (cur) {
      if (cur->isEnabled())
        return; // there is still something polling
      cur = cur->next_;
    }
    // nothing polling, check whether we can deep sleep
    TimeScheduler::checkDeepSleep();
  }
}

#if 0
#include <WString.h>

// TODO remove check stuff:
class X {
public:
  X(){}
  X(const X&) = delete;
  X& operator=(const X&) = delete;
  void run();
  static void run_static(X&) {}
};

void x0();
void x1(int);
void x2(const char*, int);
void x3(const char*, int, X&);
void x6(X&);
static X xx;

static SchedulerImpl::call_invoker<X> sel1(&X::run, xx);
static SchedulerImpl::call_invoker<> sel2{&x0};
static SchedulerImpl::call_invoker<int> sel3{&x1, 1};
static SchedulerImpl::call_invoker<const char*, int> sel4{&x2, "abc", 123};
static SchedulerImpl::call_invoker<const char*, int, X&> sel5{&x3, "abc", 123, xx};
static SchedulerImpl::call_invoker<X&> sel6{&x6, xx};

static_assert(sizeof(SchedulerImpl::call_invoker<X>) == 7, "fail");
static_assert(sizeof(SchedulerImpl::call_invoker<>) == 2, "fail");
static_assert(sizeof(SchedulerImpl::call_invoker<int>) == 5, "fail");
static_assert(sizeof(SchedulerImpl::call_invoker<const char*, int>) == 7, "fail");

static Scheduler::TaskTimingStats stats(F("Test"));
static Scheduler::TimedTask<> task1a{stats, &x0};
static Scheduler::TimedTask<int> task1b{stats, &x1, 123};
static Scheduler::UnaccountedTimedTask<const char*, int> task1c{&x2, "Abc", 123};
static Scheduler::UnaccountedTimedTask<X&> task2{&X::run_static, xx};
static Scheduler::TimedTask<X> task3{stats, &X::run, xx};
#endif

