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

#include "Scheduler.h"
#include "MQTTTopic.hpp"
#include "StringView.h"

#include <Arduino.h>

/// First registered task for task enumeration.
static Task* s_first_task = nullptr;

Task::Task(const __FlashStringHelper* name) :
  next_registered_(s_first_task),
  runtime_(name)
{
  s_first_task = this;
}

Task::~Task() {}

Scheduler::Scheduler() :
  runtime_(F("Scheduler")),
  total_runtime_(F("AllTasks"))
{}

void Scheduler::add(Task& t, unsigned long timeout_us)
{
  unsigned long new_time;
  if (is_in_loop_) {
    new_time = current_time_ + timeout_us;
  } else {
    static unsigned long s_startup_delay = 0;
    new_time = micros() + timeout_us + s_startup_delay;
    s_startup_delay = (s_startup_delay + 223500) & 0xffffffUL;  // ~220ms apart at startup, max. 1s
  }
  if (new_time == t.next_time_)
    ++new_time;   // this is for the unlikely case where re-add inside run() will use exactly the same
                  // time as before, which would remove the task from scheduling
  if (!new_time)
    new_time = 1; // 0 is special for not scheduled
  t.next_time_ = new_time;
  t.interval_ = 0;
}

void Scheduler::addRepeated(Task& t, unsigned long interval)
{
  add(t, interval);
  t.interval_ = interval;
}

void Scheduler::remove(Task& t)
{
  t.next_time_ = 0;
  t.interval_ = 0;
}

void Scheduler::loop()
{
  if (is_in_loop_)
    return; // recursive call, ERROR

  is_in_loop_ = true;
  current_time_ = micros();
  unsigned long schedule_start_time = current_time_;
  unsigned long all_task_times = 0;

  auto cur_task = s_first_task;
  while (cur_task) {
    auto task_time = cur_task->next_time_;
    auto next = cur_task->next_registered_;
    unsigned long task_start_time = micros();
    bool had_poll = cur_task->poll();
    bool had_run = false;
    if (task_time) {
      long delta = long(task_time - current_time_);
      if (delta <= 0) {
        // OK, timer expired
        cur_task->run();
        auto interval = cur_task->interval_;
        if (interval) {
          // Interval task, compute next time to run the task. In case the next time would fall
          // into this loop run, skip one call. This protects against runaway tasks that are
          // scheduled too frequently.
          while (long(task_time - current_time_) < 0)
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
        cur_task->runtime_.addRuntime(task_runtime);
      else
        cur_task->runtime_.addPolltime(task_runtime);
      all_task_times += task_runtime;
    }
    cur_task = next;
  }

  if (all_task_times) {
    // at least one task was scheduled, record how long did scheduler take
    auto sched_runtime = micros() - schedule_start_time;
    runtime_.addRuntime(sched_runtime - all_task_times);
    total_runtime_.addRuntime(sched_runtime);
  }
  is_in_loop_ = false;
}

bool Scheduler::mqttReceiveMsg(const StringView& topic, const char* /*payload*/, unsigned int /*length*/)
{
  if (topic == MQTTTopic::KwlDebugsetSchedulerGetvalues) {
    // send statistics for scheduler
    runtime_.sendStats(*this);
    total_runtime_.sendStats(*this);
    Task* t = s_first_task;
    while (t) {
      t->runtime_.sendStats(*this, t->next_time_);
      t = t->next_registered_;
    }
  } else if (topic == MQTTTopic::KwlDebugsetSchedulerResetvalues) {
    // reset maximum runtimes for all tasks
    Task* t = s_first_task;
    while (t) {
      t->runtime_.resetMaximum();
      t = t->next_registered_;
    }
  } else {
    return false;
  }
  return true;
}
