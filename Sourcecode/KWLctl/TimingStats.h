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
 * @brief Statistics for timing operation duration.
 */
#pragma once

class MessageHandler;
class __FlashStringHelper;

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
class TimingStats
{
public:
  /// Construct stats for a given task name.
  explicit TimingStats(const __FlashStringHelper* name) : name_(name) {}


  // Primary statistics for task runtime (expensive, scheduled operations).

  /// Add one runtime measurement.
  void addRuntime(unsigned long runtime);

  /// Get maximum recorded runtime.
  inline unsigned long getMaxRuntime() const { return max_runtime_; }

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

  /// Get average poll time.
  unsigned long getAvgPolltime() const;


  /*!
   * @brief Send statistics over MQTT.
   *
   * @param handler message handler where to publish the message.
   * @param next next expected task start time, if any.
   */
  void sendStats(MessageHandler& handler, unsigned long next = 0) const;

private:
  /// Task name.
  const __FlashStringHelper* name_;
  /// Maximum run time of this task in microseconds.
  unsigned long max_runtime_ = 0;
  /// Sum of runtimes of this task in microseconds.
  unsigned long sum_runtime_ = 0;
  /// Count of runtime measurements for this task.
  unsigned long count_runtime_ = 0;
  /// Count of runtime measurements "eaten out" to keep measurements in range.
  unsigned long adjust_count_runtime_ = 0;
  /// Maximum run time of this task in microseconds.
  unsigned long max_polltime_ = 0;
  /// Sum of runtimes of this task in microseconds.
  unsigned long sum_polltime_ = 0;
  /// Count of runtime measurements for this task.
  unsigned count_polltime_ = 0;
};
