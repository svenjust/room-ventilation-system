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
 * @brief Cooperative scheduler for asynchronous tasks.
 */
#pragma once

#include "Task.h"
#include "MessageHandler.h"

/*!
 * @brief Simple scheduler for cooperative multitasking.
 *
 * The scheduler works by maintaining a task list. Each task has an associated
 * next schedule time and optionally interval time. When the scheduler runs one
 * loop, it picks any expired tasks from the task list, removes them and runs
 * them. Task can re-add themselves into the scheduler, but they will be only
 * executed in the next scheduler loop. If a task specified a scheduling interval,
 * then the scheduler will automatically re-add it after execution.
 *
 * This way, it is guaranteed that all tasks get processed at some time, even if
 * there is a misbehaving task registering itself over and over with zero timeout.
 * This misbehaving task will run only at most once per scheduler loop and other
 * tasks will get their chance to run.
 *
 * Additionally, tasks can override poll() method to be called every time the
 * scheduler runs. This is for instance needed to interface with user input or
 * network handling.
 */
class Scheduler : private MessageHandler
{
public:
  Scheduler();

  /*!
   * @brief Add a task to be scheduled once.
   *
   * Timeout is corrected for task runtime, i.e., the actual timeout is task's
   * start time + timeout, not current time + timeout.
   *
   * Calling add() on an already scheduled task just updates its timeout.
   *
   * @param t Task to add.
   * @param timeout_us timeout in microseconds at which to wake up the task.
   */
  void add(Task& t, unsigned long timeout_us);

  /*!
   * @brief Add a task to be scheduled repeatedly.
   *
   * This version of add schedules the task repeatedly at the given interval.
   *
   * Calling addRepeated() on an already scheduled task just updates its timeout and interval.
   *
   * @param t Task to add.
   * @param interval interval in microseconds at which to wake up the task.
   */
  void addRepeated(Task& t, unsigned long interval);

  /*!
   * @brief Remove the specified task from scheduling.
   *
   * @param t task to remove.
   */
  void remove(Task& t);

  /*!
   * @brief Method to be called from loop() function periodically to check for expired tasks.
   */
  void loop();

private:
  virtual bool mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length) override;

  /// Current time at the time of entering into loop().
  unsigned long current_time_ = 0;
  /// Variable controlling whether we are in loop() function.
  bool is_in_loop_ = false;
  /// Runtime of the scheduler when doing useful work and scheduling some task.
  TimingStats runtime_;
  /// Total runtime of all scheduled tasks in one scheduler loop.
  TimingStats total_runtime_;
};
