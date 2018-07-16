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

#include "TaskBase.h"

/*!
 * @brief Simple scheduler for cooperative multitasking.
 *
 * The scheduler works by maintaining a task list. Each task has an associated
 * next schedule time and optionally interval time. When the scheduler runs one
 * loop, it picks any expired tasks from the task list, removes them and runs
 * them. Tasks can re-add themselves into the scheduler, but they will be only
 * executed in the next scheduler loop. If a task specified a scheduling interval,
 * then the scheduler will automatically re-add it after execution.
 *
 * This way, it is guaranteed that all tasks get processed at some time, even if
 * there is a misbehaving task registering itself over and over with zero timeout.
 * This misbehaving task will run only at most once per scheduler loop and other
 * tasks will get their chance to run.
 *
 * Additionally, polling tasks are supported. These tasks run in each run of
 * the scheduler's loop() method (and prevent deep sleep).
 *
 * Each task maintains statistics about runtime of individual invocations.
 * This can be used for debugging purposes to see which task is consuming too
 * much time. There is also a possibility to create unaccounted tasks, but
 * this is discouraged.
 *
 * Following classes are implemented by the scheduler:
 *    - TimedTask and UnaccountedTimedTask for regular tasks,
 *    - PollTask and UnaccountedPollTask for polling tasks.
 *
 * There are also two types of schedulers:
 *    - TimeScheduler for scheduling TimedTask instances only,
 *    - PollingScheduler for scheduling TimedTask and PollTask instances.
 *
 * Each task is initialized by statistics (except unaccounted tasks), task
 * function, optional class instance and optional parameters. These
 * parameters will be passed into task function. I.e., you can use the same
 * function for several tasks and differentiate by parameters.
 *
 * Examples:
 *    - TimedTask<X> task(stats, &X::timeout, x_instance) - creates a task which
 *      runs method void timeout() on instance x_instance of class X.
 *    - TimedTask<> task(stats, &voidfnc) - creates a task which runs plain function
 *      void voidfnc().
 *    - TimedTask<int> task(stats, &intfnc, intparam) - creates a task which runs
 *      plain function void intfnc(int) and passes the value intparam to it.
 *
 * Other types of tasks are created analogously.
 */
namespace Scheduler
{
  /*!
   * @brief Timed task, which fires upon timeout.
   *
   * Runtime of the task is accounted for in statistics.
   *
   * @see Scheduler namespace documentation for discussion about tasks.
   */
  template<typename... Args>
  class TimedTask : public TimedTaskBase
  {
  public:
    /*!
     * @brief Construct the task.
     *
     * @param stats statistics to update.
     * @param args arguments for task invoker (function and parameters).
     */
    template<typename... CArgs>
    TimedTask(TaskTimingStats& stats, CArgs&&... args) noexcept :
      TimedTaskBase(&invoke),
      call_invoker_(scheduler_cpp11_support::forward<CArgs>(args)...),
      stats_(stats)
    {}

    /// Get statistics for this task.
    inline TaskTimingStats& getStatistics() const noexcept { return stats_; }

  private:
    static unsigned long invoke(TaskBase& t, unsigned long start) noexcept {
      auto& instance = static_cast<TimedTask<Args...>&>(t);
      instance.call_invoker_.invoke();
      auto end = micros();
      instance.stats_.addRuntime(end - start);
      return end;
    }

    SchedulerImpl::call_invoker<Args...> call_invoker_;
    TaskTimingStats& stats_;
  };

  /*!
   * @brief Timed task, which fires upon timeout.
   *
   * Runtime of the task is not accounted for specifically, only in
   * totals of scheduler statistics. Consider using TimedTask
   * instead to get proper statistics.
   *
   * @see Scheduler namespace documentation for discussion about tasks.
   */
  template<typename... Args>
  class UnaccountedTimedTask : public TimedTaskBase
  {
  public:
    /*!
     * @brief Construct the task.
     *
     * @param args arguments for task invoker (function and parameters).
     */
    template<typename... CArgs>
    UnaccountedTimedTask(CArgs&&... args) noexcept :
      TimedTaskBase(&invoke),
      call_invoker_(scheduler_cpp11_support::forward<CArgs>(args)...)
    {}

  private:
    static unsigned long invoke(TaskBase& t, unsigned long /*start*/) {
      auto& instance = static_cast<UnaccountedTimedTask<Args...>&>(t);
      instance.call_invoker_.invoke();
      return micros();
    }

    SchedulerImpl::call_invoker<Args...> call_invoker_;
  };

  /*!
   * @brief Poll task, which is called on each scheduler run.
   *
   * Runtime of the task is accounted for in statistics.
   *
   * @see Scheduler namespace documentation for discussion about tasks.
   */
  template<typename... Args>
  class PollTask : public PollTaskBase
  {
  public:
    /*!
     * @brief Construct the task.
     *
     * @param stats statistics to update.
     * @param args arguments for task invoker (function and parameters).
     */
    template<typename... CArgs>
    PollTask(TaskPollingStats& stats, CArgs&&... args) noexcept :
      PollTaskBase(&invoke),
      call_invoker_(scheduler_cpp11_support::forward<CArgs>(args)...),
      stats_(stats)
    {}

    /// Get statistics for this task.
    inline TaskPollingStats& getStatistics() const noexcept { return stats_; }

  private:
    static unsigned long invoke(TaskBase& t, unsigned long start) noexcept {
      auto& instance = static_cast<PollTask<Args...>&>(t);
      instance.call_invoker_.invoke();
      auto end = micros();
      instance.stats_.addPolltime(end - start);
      return end;
    }

    SchedulerImpl::call_invoker<Args...> call_invoker_;
    TaskPollingStats& stats_;
  };

  /*!
   * @brief Poll task, which is called on each scheduler run.
   *
   * Runtime of the task is not accounted for specifically, only in
   * totals of scheduler statistics. Consider using PollTask
   * instead to get proper statistics.
   *
   * @see Scheduler namespace documentation for discussion about tasks.
   */
  template<typename... Args>
  class UnaccountedPollTask : public PollTaskBase
  {
  public:
    /*!
     * @brief Construct the task.
     *
     * @param args arguments for task invoker (function and parameters).
     */
    template<typename... CArgs>
    UnaccountedPollTask(CArgs&&... args) noexcept :
      PollTaskBase(&invoke),
      call_invoker_(scheduler_cpp11_support::forward<CArgs>(args)...)
    {}

  private:
    static unsigned long invoke(TaskBase& t, unsigned long /*start*/) noexcept {
      auto& instance = static_cast<UnaccountedPollTask<Args...>&>(t);
      instance.call_invoker_.invoke();
      return micros();
    }

    SchedulerImpl::call_invoker<Args...> call_invoker_;
  };

  /*!
   * @brief Function for deep sleep, if there are no tasks to run.
   *
   * @param us number of microseconds to deep sleep.
   */
  using DeepSleepCallback = void(*)(unsigned long us);

  /*!
   * @brief Task scheduler for timed tasks.
   *
   * This scheduler does not support polling. Use PollingScheduler to
   * also schedule polling tasks.
   *
   * @see Scheduler namespace documentation for discussion about tasks.
   */
  class TimeScheduler
  {
  public:
    /*!
     * @brief Construct the scheduler.
     *
     * @param deep_sleep (optional) function to enter deep sleep if
     *    there are no tasks to run.
     */
    TimeScheduler(DeepSleepCallback deep_sleep = nullptr) noexcept :
      deep_sleep_(deep_sleep)
    {}

    /// Method to call in loop() to process tasks.
    void loop() noexcept;

  protected:
    /// Run normal timed tasks.
    unsigned long runTimedTasks() noexcept;
    /// Check whether deep sleep is necessary and do deep sleep.
    virtual void checkDeepSleep() noexcept;

    /// Function for deep sleep, if there are no tasks to run.
    DeepSleepCallback deep_sleep_;
  };

  /*!
   * @brief Task scheduler for timed and polling tasks.
   *
   * @see Scheduler namespace documentation for discussion about tasks.
   */
  class PollingScheduler : private TimeScheduler
  {
  public:
    /*!
     * @brief Construct the scheduler.
     *
     * @param deep_sleep (optional) function to enter deep sleep if
     *    there are no tasks to run.
     */
    PollingScheduler(DeepSleepCallback deep_sleep = nullptr) noexcept :
      TimeScheduler(deep_sleep)
    {}

    /// Method to call in loop() to process tasks.
    void loop() noexcept;

  protected:
    /// Run polling tasks.
    unsigned long runPollTasks() noexcept;

    virtual void checkDeepSleep() noexcept override;
  };
}
