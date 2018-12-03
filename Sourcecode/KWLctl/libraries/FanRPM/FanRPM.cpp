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

#include "FanRPM.h"

#include <Arduino.h>

FanRPM::FanRPM() noexcept {
  memset(measurements_, 0, sizeof(measurements_));
}

void FanRPM::interrupt() noexcept {
  // perform one measurement
  unsigned long timer = micros();
  if (!last_time_) {
    last_time_ = timer;  // no measurements yet
    return;
  }
  unsigned long measurement = timer - last_time_;
  if (measurement > (60000000UL / MIN_RPM)) {
    // assume fan stopped - it is too slow (more than 1s between signals)
    last_time_ = 0;
    last_ = 0;
    valid_ = false;
    return;
  }
  if (measurement < (60000000UL / MAX_RPM)) {
    // assume outlier (e.g., double signal activation, random noise, etc.)
    return;
  }
  last_time_ = timer;

  if (last_) {
    // now check for validity of the new measurement (not more than 25% off)
    auto diff = measurement >> 2;
    auto low_bound = last_ - diff;
    if (measurement < low_bound) {
      last_ = low_bound;
      return;
    }
    auto high_bound = last_ + diff;
    if (measurement > high_bound) {
      last_ = high_bound;
      return;
    }
  }

  // measurement OK, enter it in the list
  unsigned long old = measurements_[index_];
  measurements_[index_] = measurement;
  sum_ = sum_ + measurement - old;
  index_ = (index_ + 1) & (MAX_MEASUREMENTS - 1);
  valid_ = true;
  last_ = measurement;
}

int FanRPM::getSpeed() noexcept
{
  auto valid = valid_;
  auto sum = sum_;
  auto last_time = last_time_;
  for (;;) {
    // Now re-read all data and check for change, if changed, re-read again.
    // This prevents the need to disable interrupts for reading data.
    const auto new_valid = valid_;
    const auto new_sum = sum_;
    const auto new_last_time = last_time_;
    if (valid == new_valid && sum == new_sum && last_time == new_last_time)
      break;  // data read consistently
    // interrupt in-between changed state, re-read
    valid = new_valid;
    sum = new_sum;
    last_time = new_last_time;
  }

  if (!valid)
    return 0;

  // Create pseudo-measurement to check whether fan has stopped
  // w/o detection in the interrupt routine.
  const unsigned long measurement = micros() - last_time;
  if (measurement > (60000000UL / MIN_RPM)) {
    // assume fan stopped - it is too slow (more than 1s between signals)
    noInterrupts();
    last_time_ = 0;
    last_ = 0;
    valid_ = false;
    interrupts();
    return 0;
  }

  // Captured value is effectively sum of MAX_MEASUREMENT measurements, return average.
  if (sum)
    return int((60000000UL * MAX_MEASUREMENTS) / sum);
  else
    return 0;
}

void FanRPM::dump(Print& out) noexcept {
  if (!valid_)
    out.print(F("INVALID: "));
  out.print(F("Last: "));
  out.print(last_);
  out.print('@');
  out.print(last_time_);
  out.print(F(", index "));
  out.print(index_);
  out.print(F(": "));
  for (int i = 0; i < MAX_MEASUREMENTS; ++i) {
    out.print(measurements_[i]);
    out.print(';');
  }
  out.println(sum_);
}
