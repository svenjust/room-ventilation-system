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

#include <Arduino.h>

/// First registered task for task enumeration.
static Task* s_first_task = nullptr;

Task::Task(const char* name) :
  name_(name),
  next_registered_(s_first_task)
{
  s_first_task = this;
}

void Task::addRuntime(unsigned long runtime)
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

unsigned long Task::getAvgRuntime() const {
  unsigned long count = (count_runtime_ - adjust_count_runtime_);
  if (count)
    return sum_runtime_ / count;
  else
    return 0;
}

void Scheduler::add(Task& t, unsigned long timeout_us)
{
  unsigned long new_time;
  if (is_in_loop_)
    new_time = current_time_ + timeout_us;
  else
    new_time = micros() + timeout_us;
  t.next_time_ = new_time;
  t.interval_ = 0;

  if (!t.is_in_list_) {
    if (queue_last_)
      queue_last_->next_ = &t;
    else
      queue_ = &t;
    t.next_ = nullptr;
    queue_last_ = &t;
    t.is_in_list_ = true;
  }
}

void Scheduler::addRepeated(Task& t, unsigned long interval)
{
  add(t, interval);
  t.interval_ = interval;
}

void Scheduler::loop()
{
  enum {
    MAX_TASKS = 64  // maximum # of tasks to check in one loop
  };

  if (is_in_loop_)
    return; // recursive call, ERROR
  is_in_loop_ = true;
  current_time_ = micros();
  unsigned long task_start_time = current_time_;

  Task* last_task = nullptr;
  auto cur_task = next_task_;
  bool switched = false;
  for (byte i = 0; i < MAX_TASKS; ++i) {
    if (!cur_task) {
      if (!switched && queue_) {
        // pick new tasks from the queue, if any
        if (last_task) {
          last_task->next_ = queue_;
        } else {
          next_task_ = queue_;
          last_task = nullptr;
        }
        cur_task = queue_;
        queue_ = queue_last_ = nullptr;
        switched = true;
      } else {
        break; // no mote tasks to schedule at this time
      }
    }

    auto task_time = cur_task->next_time_;
    long delta = long(task_time - current_time_);
    auto next = cur_task->next_;
    if (delta > 0) {
      // not expired yet, try next task in the list
      last_task = cur_task;
      cur_task = next;
      continue;
    }

    // OK, timer expired
    auto interval = cur_task->interval_;
    if (interval) {
      // Interval task, compute next time to run the task. In case the next time would fall
      // into this loop run, skip one call. This protects against runaway tasks that are
      // scheduled too frequently.
      while (long(task_time - current_time_) < 0)
        task_time += interval;
      cur_task->run();
      cur_task->next_time_ = task_time;
    } else {
      // Regular task, remove the task from the list. It has to be explicitly re-added.
      if (last_task)
        last_task->next_ = next;
      else
        next_task_ = next;
      cur_task->is_in_list_ = false;
      cur_task->run();
      if (!cur_task->is_in_list_)
        cur_task->next_time_ = 0;
    }
    unsigned long task_end_time = micros();
    auto task_runtime = task_end_time - task_start_time;
    cur_task->addRuntime(task_runtime);
    task_start_time = task_end_time;
    cur_task = next;
  }

  if (queue_) {
    // integrate tasks from the pending queue into task list
    last_task = nullptr;
    cur_task = next_task_;
    while (cur_task) {
      last_task = cur_task;
      cur_task = cur_task->next_;
    }
    if (last_task)
      last_task->next_ = queue_;
    else
      next_task_ = queue_;
  }
  is_in_loop_ = false;
}

bool Scheduler::mqttReceiveMsg(const StringView& topic, const char* /*payload*/, unsigned int /*length*/)
{
  if (topic == MQTTTopic::KwlDebugsetSchedulerGetvalues) {
    // send statistics for scheduler
    Task* t = s_first_task;
    char buffer[80];
    while (t) {
      snprintf(buffer, sizeof(buffer), "%s: max=%lu, avg=%lu, cnt=%lu-%lu, next=%lu",
               t->name_, t->max_runtime_, t->getAvgRuntime(),
               t->count_runtime_, t->adjust_count_runtime_, t->next_time_);
      publish(MQTTTopic::KwlDebugstateScheduler, buffer, false);
      t = t->next_registered_;
    }
  } else {
    return false;
  }
  return true;
}
