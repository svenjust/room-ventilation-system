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

#include <Arduino.h>

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
    }
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
