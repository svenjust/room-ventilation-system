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
 * @brief Timing statistics for scheduler tasks.
 */
#pragma once

class __FlashStringHelper;

namespace Scheduler
{
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
    /// Iterator over statistics.
    class iterator
    {
    public:
      TaskTimingStats& operator*() noexcept { return *cur_; }
      TaskTimingStats* operator->() noexcept { return cur_; }

      /// Move to the next task.
      iterator& operator++() noexcept { cur_ = cur_->next_; return *this; }

    private:
      friend class TaskTimingStats;
      explicit iterator(TaskTimingStats* ptr) noexcept : cur_(ptr) {}
      friend bool operator==(const iterator& l, const iterator& r) noexcept { return l.cur_ == r.cur_; }
      friend bool operator!=(const iterator& l, const iterator& r) noexcept { return l.cur_ != r.cur_; }
      TaskTimingStats* cur_;
    };

    /// Construct stats for a given task name.
    explicit TaskTimingStats(const __FlashStringHelper* name) noexcept;

    /// Get statistics name.
    const __FlashStringHelper* getName() const noexcept { return name_; }

    /// Add one runtime measurement.
    void addRuntime(unsigned long runtime) noexcept;

    /// Get maximum recorded runtime.
    inline unsigned long getMaxRuntime() const noexcept { return max_runtime_; }

    /// Get maximum recorded runtime since start.
    unsigned long getMaxRuntimeSinceStart() const noexcept;

    /// Get average runtime.
    unsigned long getAvgRuntime() const noexcept;

    /// Get total measurement count.
    inline unsigned long getMeasurementCount() const noexcept { return count_runtime_; }

    /// Get number of measurements consolidated in order not to overflow long counters.
    unsigned long getConsolidatedMeasurementCount() const noexcept { return adjust_count_runtime_; }

    /*!
     * @brief Serialize statistics to a buffer.
     *
     * @param buffer,size buffer where to materialize the string (should be >=140B).
     */
    void toString(char* buffer, unsigned size) const noexcept;

    /// Reset maximum.
    void resetMaximum() noexcept;

    /// Get iterator to the first task.
    static iterator begin() noexcept { return iterator(s_first_stat_); }

    /// Get iterator past the last task.
    static iterator end() noexcept { return iterator(nullptr); }

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
    /// Next task statistics in the list.
    TaskTimingStats* next_;
    /// First statistics.
    static TaskTimingStats* s_first_stat_;
  };

  /*!
   * @brief Statistics for timing poll operation duration.
   *
   * Typically, measurements are in microseconds, but the unit is not
   * important for the purpose of the statistics.
   *
   * The implementation accounts for numeric overflows and consolidates
   * sum and count appropriately, so a reliable average can be built over
   * time.
   */
  class TaskPollingStats
  {
  public:
    /// Iterator over statistics.
    class iterator
    {
    public:
      TaskPollingStats& operator*() noexcept { return *cur_; }
      TaskPollingStats* operator->() noexcept { return cur_; }

      /// Move to the next task.
      iterator& operator++() noexcept { cur_ = cur_->next_; return *this; }

    private:
      friend class TaskPollingStats;
      explicit iterator(TaskPollingStats* ptr) noexcept : cur_(ptr) {}
      friend bool operator==(const iterator& l, const iterator& r) noexcept { return l.cur_ == r.cur_; }
      friend bool operator!=(const iterator& l, const iterator& r) noexcept { return l.cur_ != r.cur_; }
      TaskPollingStats* cur_;
    };

    /// Construct stats for a given task name.
    explicit TaskPollingStats(const __FlashStringHelper* name) noexcept;

    /// Get statistics name.
    const __FlashStringHelper* getName() const noexcept { return name_; }

    /// Add time spent in polling.
    void addPolltime(unsigned long polltime) noexcept;

    /// Get maximum recorded poll time.
    inline unsigned long getMaxPolltime() const noexcept { return max_polltime_; }

    /// Get maximum recorded polltime since start.
    unsigned long getMaxPolltimeSinceStart() const noexcept;

    /// Get average poll time.
    unsigned long getAvgPolltime() const noexcept;

    /*!
     * @brief Serialize statistics to a buffer.
     *
     * @param buffer,size buffer where to materialize the string (should be >=140B).
     */
    void toString(char* buffer, unsigned size) const noexcept;

    /// Reset maximum.
    void resetMaximum() noexcept;

    /// Get iterator to the first task.
    static iterator begin() noexcept { return iterator(s_first_stat_); }

    /// Get iterator past the last task.
    static iterator end() noexcept { return iterator(nullptr); }

  private:
    /// Task name.
    const __FlashStringHelper* name_;
    /// Maximum poll time of this task in microseconds.
    unsigned long max_polltime_ = 0;
    /// Maximum poll time of this task since beginning at reset.
    unsigned long max_polltime_since_start_ = 0;
    /// Sum of polltimes of this task in microseconds.
    unsigned long sum_polltime_ = 0;
    /// Count of polltime measurements for this task.
    unsigned count_polltime_ = 0;
    /// Next task statistics in the list.
    TaskPollingStats* next_;
    /// First statistics.
    static TaskPollingStats* s_first_stat_;
  };
}
