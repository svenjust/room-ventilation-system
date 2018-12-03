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

#include "TaskBase.h"

namespace Scheduler
{
  bool TaskBase::s_is_in_loop_ = false;
  unsigned long TaskBase::s_scheduler_current_time_ = 0;
  TimedTaskBase* TimedTaskBase::s_first_task_ = nullptr;
  PollTaskBase* PollTaskBase::s_first_task_ = nullptr;

  void TimedTaskBase::runRepeated(unsigned long timeout, unsigned long interval) noexcept
  {
    unsigned long new_time;
    if (s_is_in_loop_) {
      new_time = s_scheduler_current_time_ + timeout;
    } else {
      static unsigned long s_startup_delay = 0;
      new_time = micros() + timeout + s_startup_delay;
      s_startup_delay = (s_startup_delay + 223500) & 0xffffffUL;  // ~220ms apart at startup, max. 1s
    }
    if (new_time == next_time_)
      ++new_time;   // this is for the unlikely case where re-add inside run() will use exactly the same
                    // time as before, which would remove the task from scheduling
    if (!new_time)
      new_time = 1; // 0 is special for not scheduled
    next_time_ = new_time;
    interval_ = interval;
  }

  void TimedTaskBase::cancel() noexcept
  {
    next_time_ = interval_ = 0;
  }
}
