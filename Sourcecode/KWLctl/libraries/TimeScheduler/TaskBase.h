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
 * @brief Simple scheduler for cooperative multitasking.
 */
#pragma once

#include "TaskTimingStats.h"
#include "TimeSchedulerHelpers.h"

// forward to prevent including large headers
extern "C" unsigned long micros(void);

namespace Scheduler
{
  /*!
   * @brief Base class for all tasks.
   */
  class TaskBase
  {
  public:
    using invoker_type = unsigned long (*)(TaskBase&, unsigned long);

    TaskBase() = delete;
    TaskBase(const TaskBase&) = delete;
    TaskBase& operator=(const TaskBase&) = delete;

    explicit TaskBase(invoker_type invoker) noexcept : invoker_(invoker) {}

    /// Invoke this task.
    unsigned long invoke(unsigned long start) noexcept
    {
      return invoker_(*this, start);
    }

  protected:
    friend class TimeScheduler;
    friend class PollingScheduler;

    /// Invoker for this task, called with thisptr and current time, must return new time (micros()).
    invoker_type invoker_;

    /// Flag whether the scheduler is currently active.
    static bool s_is_in_loop_;
    /// Current time at which the scheduler loop started.
    static unsigned long s_scheduler_current_time_;
  };

  /*!
   * @brief Base class for all timed tasks.
   */
  class TimedTaskBase : protected TaskBase
  {
  protected:
    explicit TimedTaskBase(invoker_type invoker) noexcept :
      TaskBase(invoker),
      next_(s_first_task_)
    {
      s_first_task_ = this;
    }

  public:
    /*!
     * @brief Run this task repeatedly.
     *
     * @param timeout timeout in microseconds.
     * @param interval interval in microseconds.
     */
    void runRepeated(unsigned long timeout, unsigned long interval) noexcept;

    /*!
     * @brief Run this task repeatedly.
     *
     * @param interval interval in microseconds.
     */
    void runRepeated(unsigned long interval) noexcept { runRepeated(interval, interval); }

    /*!
     * @brief Run this task once after specified timeout passes.
     *
     * @param timeout timeout in microseconds.
     */
    void runOnce(unsigned long timeout) noexcept { runRepeated(timeout, 0); }

    /// Cancel this task (it won't run anymore).
    void cancel() noexcept;

    /*!
     * @brief Set interval for the task inside run() method.
     *
     * After the run() method completes, the task will be scheduled with this
     * interval (first time after run() returns).
     *
     * @param interval scheduling interval in microseconds.
     */
    void setInterval(unsigned long interval) noexcept { interval_ = interval; }

    /// Get scheduling interval or 0 if no interval set.
    unsigned long getInterval() const noexcept { return interval_; }

    /*!
     * @brief Get the time at which the task was set to timeout.
     *
     * Use this method inside of run() to get time information. The time
     * returned is close to the current time.
     *
     * Resetting the timeout using runOnce() or runRepeated() will set
     * the time to the next timeout.
     *
     * @return time in microseconds or 0 if not scheduled.
     */
    inline unsigned long getScheduleTime() const noexcept { return next_time_; }

  private:
    friend class TimeScheduler;

    /// Next time at which to react to this task.
    unsigned long next_time_ = 0;
    /// Interval with which to schedule this task.
    unsigned long interval_ = 0;
    /// Next registered task.
    TimedTaskBase* next_;
    /// First registered task.
    static TimedTaskBase* s_first_task_;
  };

  /*!
   * @brief Base class for all poll tasks.
   */
  class PollTaskBase : protected TaskBase
  {
  protected:
    explicit PollTaskBase(invoker_type invoker) noexcept :
      TaskBase(invoker), next_(s_first_task_)
    {
      s_first_task_ = this;
    }

    /// Check if the task is enabled.
    bool isEnabled() const noexcept { return enabled_; }

    /// Disable this task (it won't run anymore).
    void disable() noexcept { enabled_ = false; }

    /// Enable this task.
    void enable() noexcept { enabled_ = true; }

  private:
    friend class PollingScheduler;

    /// Next registered poll task.
    PollTaskBase* next_;
    /// Enabled flag.
    bool enabled_ = true;
    /// First registered poll task.
    static PollTaskBase* s_first_task_;
  };
}
