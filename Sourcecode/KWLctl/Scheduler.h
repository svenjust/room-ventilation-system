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

class Scheduler;

/*!
 * @brief Base class for all tasks schedulable by a scheduler.
 */
class Task
{
public:
  Task() {}

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
};

/*!
 * @brief Simple scheduler for cooperative multitasking.
 *
 * The scheduler works by maintaining two tasks lists - a queue of pending tasks
 * and a list of current tasks. When the scheduler runs one loop, it picks any
 * expired tasks from the current task list, removes them and runs them. Tasks
 * can re-add themselves into the scheduler, but they will end up in pending
 * queue.
 *
 * When the entire current task list is processed in one loop() call, the pending
 * queue becomes the current task list and unless it was once already switched to
 * pending task queue, it will be processed in the same loop call.
 *
 * If not entire current task list is processed in one loop() call (typical), then
 * the pending queue is added at the end of the current task list and will be
 * processed in the next scheduler run.
 *
 * This way, it is guaranteed that all tasks get processed at some time, even if
 * there is a misbehaving task registering itself over and over with zero timeout.
 * This misbehaving task will run only at most twice per scheduler loop and other
 * tasks will get their chance to run.
 */
class Scheduler
{
public:
  /*!
   * @brief Add a task to be scheduled once.
   *
   * Timeout is corrected for task runtime, i.e., the actual timeout is task's
   * start time + timeout, not current time + timeout.
   *
   * Calling add() on a task already in the list just updates its timeout.
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
   * Calling addRepeated() on a task already in the list just updates its interval.
   *
   * @param t Task to add.
   * @param interval interval in microseconds at which to wake up the task.
   */
  void addRepeated(Task& t, unsigned long interval);

  /*!
   * @brief Method to be called from loop() function periodically to check for expired tasks.
   */
  void loop();

private:
  /// Current time at the time of entering into loop().
  unsigned long current_time_ = 0;
  /// Next task to activate when next_time_ expires.
  Task* next_task_ = nullptr;
  /// Queue of tasks where new tasks register themselves with add().
  Task* queue_ = nullptr;
  /// Last registered element in the queue, if any.
  Task* queue_last_ = nullptr;
  /// Variable controlling whether we are in loop() function.
  bool is_in_loop_ = false;
};
