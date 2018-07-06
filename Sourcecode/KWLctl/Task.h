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
 * @brief Task definition for a cooperative scheduler for asynchronous tasks.
 */
#pragma once

#include "TimingStats.h"

class Scheduler;

/*!
 * @brief Base class for all tasks schedulable by a scheduler.
 */
class Task
{
public:
  Task(const char* name);

  virtual ~Task() {}

  /*!
   * @brief Initialize the task when the scheduler starts.
   */
  virtual void init() {}

  /*!
   * @brief Timer expired, run the task.
   *
   * This method is called by the scheduler after specified time expiry interval.
   */
  virtual void run() = 0;

protected:
  /*!
   * @brief Get the time at which the task was set to timeout.
   *
   * Use this method inside of run() to get time information. The time
   * returned is close to the current time.
   *
   * @return time in microseconds.
   */
  inline unsigned long getScheduleTime() { return next_time_; }

private:
  friend class Scheduler;

  /// Next time at which to react to this task.
  unsigned long next_time_ = 0;
  /// Interval with which to schedule this task.
  unsigned long interval_ = 0;
  /// Next task in scheduler's list. Maintained by scheduler.
  Task* next_ = nullptr;
  /// Set to true, if the task is already in scheduler's list.
  bool is_in_list_ = false;

  /// Next registered task in global list. Maintained by constructor.
  Task* next_registered_ = nullptr;
  /// Run time of this task.
  TimingStats runtime_;
};

