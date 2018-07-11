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
 * Optionally, tasks can specify poll() method to be called every time the
 * scheduler runs. This is for instance needed to interface with user input or
 * network handling.
 */
#pragma once

class __FlashStringHelper;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"

/*!
 * @brief Statistics for timing operation duration.
 *
 * Typically, measurements are in microseconds, but the unit is not
 * important for the purpose of the statistics.
 *
 * The implementation accounts for numeric overflows and consolidates
 * sum and count appropriately, so a reliable average can be built over
 * time.
 */
class TaskTimingStats
{
public:
  /// Construct stats for a given task name.
  explicit TaskTimingStats(const __FlashStringHelper* name) : name_(name) {}


  // Primary statistics for task runtime (expensive, scheduled operations).

  /// Add one runtime measurement.
  void addRuntime(unsigned long runtime);

  /// Get maximum recorded runtime.
  inline unsigned long getMaxRuntime() const { return max_runtime_; }

  /// Get maximum recorded runtime since start.
  unsigned long getMaxRuntimeSinceStart() const;

  /// Get average runtime.
  unsigned long getAvgRuntime() const;

  /// Get total measurement count.
  inline unsigned long getMeasurementCount() const { return count_runtime_; }

  /// Get number of measurements consolidated in order not to overflow long counters.
  unsigned long getConsolidatedMeasurementCount() const { return adjust_count_runtime_; }


  // Secondary statistics for task poll time (quick operations, react to network and input).

  /// Add time spent in polling.
  void addPolltime(unsigned long polltime);

  /// Get maximum recorded poll time.
  inline unsigned long getMaxPolltime() const { return max_polltime_; }

  /// Get maximum recorded polltime since start.
  unsigned long getMaxPolltimeSinceStart() const;

  /// Get average poll time.
  unsigned long getAvgPolltime() const;


  /*!
   * @brief Serialize statistics to a buffer.
   *
   * @param buffer,size buffer where to materialize the string (should be >=140B).
   */
  void toString(char* buffer, unsigned size) const;

  /// Reset maxima.
  void resetMaximum();

private:
  /// Task name.
  const __FlashStringHelper* name_;
  /// Maximum run time of this task in microseconds.
  unsigned long max_runtime_ = 0;
  /// Maximum run time of this task since beginning at reset.
  unsigned long max_runtime_since_start_ = 0;
  /// Sum of runtimes of this task in microseconds.
  unsigned long sum_runtime_ = 0;
  /// Count of runtime measurements for this task.
  unsigned long count_runtime_ = 0;
  /// Count of runtime measurements "eaten out" to keep measurements in range.
  unsigned long adjust_count_runtime_ = 0;
  /// Maximum poll time of this task in microseconds.
  unsigned long max_polltime_ = 0;
  /// Maximum poll time of this task since beginning at reset.
  unsigned long max_polltime_since_start_ = 0;
  /// Sum of polltimes of this task in microseconds.
  unsigned long sum_polltime_ = 0;
  /// Count of polltime measurements for this task.
  unsigned count_polltime_ = 0;
};

class Task;

/*!
 * @brief Subtask schedulable by a scheduler.
 *
 * Subtask doesn't have any private statistics, but it can be scheduled
 * in addition to the parent task.
 *
 * @see Task
 */
class Subtask
{
public:
  Subtask(const Subtask&) = delete;
  Subtask& operator=(const Subtask&) = delete;

  /*!
   * @brief Construct a subtask.
   *
   * @param parent parent task (to use statistics from).
   * @param obj object instance on which to invoke run and poll methods.
   * @param run run method called upon timeout expiration.
   * @param poll poll method called every time on loop.
   */
  template<typename T>
  Subtask(Task& parent, T& obj, void (T::*run)(), void (T::*poll)());

  /*!
   * @brief Construct a subtask.
   *
   * @param parent parent task (to use statistics from).
   * @param obj object instance on which to invoke run and poll methods.
   * @param run run method called upon timeout expiration (defaults to T::run()).
   */
  template<typename T>
  Subtask(Task& parent, T& obj, void (T::*run)() = &T::run);

  /*!
   * @brief Run this task repeatedly.
   *
   * @param timeout timeout in microseconds.
   * @param interval interval in microseconds.
   */
  void runRepeated(unsigned long timeout, unsigned long interval);

  /*!
   * @brief Run this task repeatedly.
   *
   * @param interval interval in microseconds.
   */
  void runRepeated(unsigned long interval) { runRepeated(interval, interval); }

  /*!
   * @brief Run this task once after specified timeout passes.
   *
   * @param timeout timeout in microseconds.
   */
  void runOnce(unsigned long timeout) { runRepeated(timeout, 0); }

  /// Cancel this task (it won't run anymore).
  void cancel();

  /// Get statistics of this task.
  TaskTimingStats& getStatistics() { return stats_; }

protected:
  /*!
   * @brief Get the time at which the task was set to timeout.
   *
   * Use this method inside of run() to get time information. The time
   * returned is close to the current time.
   *
   * @return time in microseconds or 0 if not scheduled.
   */
  inline unsigned long getScheduleTime() { return next_time_; }

  /// Check if the task is scheduled repeatedly.
  bool isRepeated() { return interval_ != 0; }

private:
  friend class Task;

  Subtask(TaskTimingStats& stats, void* instance, void (*invoker)(void*), void (*poller)(void*));

  template<typename T>
  Subtask(TaskTimingStats& stats, T& obj, void (T::*run)(), void (T::*poll)()) :
    Subtask(stats, reinterpret_cast<void*>(&obj),
            reinterpret_cast<void(*)(void*)>(obj.*run),
            reinterpret_cast<void(*)(void*)>(obj.*poll))
  {}

  template<typename T>
  Subtask(TaskTimingStats& stats, T& obj, void (T::*run)()) :
    Subtask(stats, reinterpret_cast<void*>(&obj),
            reinterpret_cast<void(*)(void*)>(obj.*run),
            nullptr)
  {}

  /// Invoke run method of this task.
  void invoke();

  /// Invoke poll method of this task, if any.
  bool poll();

  /// Reference to statistics to maintain.
  TaskTimingStats& stats_;

  /// Invoker function.
  void (*const invoker_)(void*);
  /// Poller function.
  void (*const poller_)(void*);
  /// Instance pointer.
  void* instance_ptr_;

  /// Next time at which to react to this task.
  unsigned long next_time_ = 0;
  /// Interval with which to schedule this task.
  unsigned long interval_ = 0;

  /// Next registered task in global list. Maintained by constructor.
  Subtask* next_ = nullptr;

  /// First registered task in global list.
  static Subtask* s_first_task_;
};

/*!
 * @brief Task schedulable by a scheduler.
 *
 * Call Scheduler::loop() to run tasks.
 */
class Task : public Subtask
{
public:
  /// Iterator over tasks.
  class iterator
  {
  public:
    Subtask& operator*() { return *cur_; }
    Subtask* operator->() { return cur_; }

    /// Move to the next task.
    iterator& operator++();

  private:
    friend class Task;
    explicit iterator(Subtask* task);
    void find_next_valid();
    friend bool operator==(const iterator& l, const iterator& r) { return l.cur_ == r.cur_; }
    friend bool operator!=(const iterator& l, const iterator& r) { return l.cur_ != r.cur_; }
    Subtask* cur_;
  };

  /*!
   * @brief Construct a task.
   *
   * @param name name of this task (for the statistics).
   * @param obj object instance on which to invoke run and poll methods.
   * @param run run method called upon timeout expiration.
   * @param poll poll method called every time on loop.
   */
  template<typename T>
  Task(const __FlashStringHelper* name, T& obj, void (T::*run)(), void (T::*poll)()) :
    Subtask(real_stats_, obj, run, poll),
    real_stats_(name)
  {}

  /*!
   * @brief Construct a task.
   *
   * @param name name of this task (for the statistics).
   * @param obj object instance on which to invoke run and poll methods.
   * @param run run method called upon timeout expiration (defaults to T::run()).
   */
  template<typename T>
  Task(const __FlashStringHelper* name, T& obj, void (T::*run)() = &T::run) :
    Subtask(real_stats_, obj, run),
    real_stats_(name)
  {}

  /// Get iterator to the first task.
  static iterator begin();

  /// Get iterator past the last task.
  static iterator end();

  /// Get statistics indicating how long the scheduler as such has run.
  static TaskTimingStats& getSchedulerStatistics();

  /// Get statistics indicating how long all the tasks in one scheduler run have run.
  static TaskTimingStats& getAllTasksStatistics();

  /// Run any expired tasks and execute any polling. Called from main loop() function.
  static void loop();

  /// Reset runtime maxima of all tasks.
  static void resetAllMaxima();

protected:
  /*!
   * @brief Set interval for the task inside run() method.
   *
   * After the run() method completes, the task will be scheduled with this
   * interval (first time after run() returns).
   *
   * @param interval scheduling interval in microseconds.
   */
  void setInterval(unsigned long interval) { interval_ = interval; }

private:
  friend class Subtask;

  /// Run time of this task.
  TaskTimingStats real_stats_;
};

template<typename T>
Subtask::Subtask(Task& parent, T& obj, void (T::*run)(), void (T::*poll)()) :
  Subtask(parent.stats_, reinterpret_cast<void*>(obj),
          reinterpret_cast<void(*)(void*)>(obj.*run),
          reinterpret_cast<void(*)(void*)>(obj.*poll))
{}

template<typename T>
Subtask::Subtask(Task& parent, T& obj, void (T::*run)()) :
  Subtask(parent.stats_, reinterpret_cast<void*>(obj),
          reinterpret_cast<void(*)(void*)>(obj.*run),
          nullptr)
{}

#pragma GCC diagnostic pop
