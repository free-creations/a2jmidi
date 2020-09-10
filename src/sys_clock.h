
/*
 * File: clock.h
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
 * The module `sysClock`aims to simplify the use of the Standard Library <chrono> unit.
 */
namespace sysClock {

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using SysTimeUnits = SteadyClock::duration;
using Microseconds = std::chrono::microseconds;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-identifier-naming"
const auto now = SteadyClock::now;
#pragma clang diagnostic pop

inline Microseconds toMicroseconds(SysTimeUnits duration){
  return std::chrono::duration_cast<Microseconds>(duration);
}

} // namespace sysClock
#endif //A_J_MIDI_SYS_CLOCK_H

