/*
 * File: sys_clock.h
 *
 *
 * Copyright 2020 Harald Postner <Harald at free_creations.de>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef A_J_MIDI_SYS_CLOCK_H
#define A_J_MIDI_SYS_CLOCK_H

#include <chrono>

/**
 * This module aims to simplify the use of the <chrono> Standard Library.
 */
namespace sysClock {

using Clockwork = std::chrono::high_resolution_clock;
using TimePoint = Clockwork::time_point;
using SysTimeUnits = Clockwork::duration;
/**
 * @return a time point representing the current point in time.
 */
inline TimePoint now(){
  return Clockwork::now();
}
/**
 * Converts the given duration into a floating point number representing the duration
 * in microseconds.
 * @param duration - a duration in system time units.
 * @return a floating point number representing the duration in microseconds.
 */
inline double toMicrosecondFloat(const SysTimeUnits& duration){
  return std::chrono::duration<double, std::micro>(duration).count();
}
/**
 * Converts the given duration to system time-units.
 * @param durationMicroseconds - duration in microseconds
 * @return duration in system time-units.
 */
inline SysTimeUnits toSysTimeUnits(const float durationMicroseconds){
  auto asChronoUs = std::chrono::duration<float, std::micro>(durationMicroseconds);
  return std::chrono::duration_cast<SysTimeUnits>(asChronoUs);
}

/**
 * The number of ticks in one second.
 */
constexpr long TICKS_PER_SECOND = sysClock::SysTimeUnits::period::den;
} // namespace sysClock
#endif //A_J_MIDI_SYS_CLOCK_H

