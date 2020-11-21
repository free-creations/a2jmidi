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
#ifndef A_J_MIDI_SRC_A2JMIDI_CLOCK_H
#define A_J_MIDI_SRC_A2JMIDI_CLOCK_H
#include <memory>
namespace a2jmidi {
/**
 * An abstract class representing a clock.
 * This class permits to define an application specific clock.
 * It can be instantiated to either the _internal JackClock_ or,
 * for test purposes, to a _TestClock_. The _TestClock_ permits to perform tests
 * without a running JACK server.
 */
class Clock {
public:
  virtual ~Clock() = default;
  /**
   * The estimated current time in frames (or some replacing concept for tests).
   * @return the estimated current time in in frames.
   */
  virtual long now() = 0;
};
/**
 * A smart pointer that owns and manages an Clock-object through a pointer and
 * disposes of that object when the ClockPtr goes out of scope.
 */
using ClockPtr = std::unique_ptr<Clock>;

using TimePoint = long;


} // namespace a2jmidi
#endif // A_J_MIDI_SRC_A2JMIDI_CLOCK_H
