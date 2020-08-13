/*
 * File: alsa_listener.h
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
#ifndef A_J_MIDI_SRC_ALSA_LISTENER_H
#define A_J_MIDI_SRC_ALSA_LISTENER_H

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace alsa_listener {

class MidiEvent;
using Sys_clock = std::chrono::steady_clock;
using Std_time_point = std::chrono::steady_clock::time_point;
using MidiEvent_ptr = std::unique_ptr<MidiEvent>;
using FutureMidiEvent = std::future<MidiEvent_ptr>;

/**
 * When a listener process is stopped, it throws
 * the InterruptedException.
 */
class InterruptedException : public std::future_error {
public:
  InterruptedException()
      : std::future_error(std::future_errc::broken_promise){};
};

/**
 * Terminate all listeners that wait for incoming Midi events.
 * Once the listeners are stopped, they cannot be restarted.
 */
void terminateListening();

/**
 * Indicates whether the listener processes shall carry on waiting for incoming Midi events.
 * @return true - if the listener processes shall carry on,
 *         false - if the listener processes shall stop.
 */
bool carryOnListening();

/**
 * Indicates whether the given future is ready to deliver a result.
 * @param futureMidiEvent - a FutureMidiEvent that might be ready
 * @return true - if there is a result, false - if the future is still waiting for an incoming Midi event.
 */
inline bool isReady(const FutureMidiEvent &futureMidiEvent) {
  auto status = futureMidiEvent.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

class MidiEvent {
private:
  FutureMidiEvent _next;
  const int _midiValue;
  const Std_time_point _timeStamp;

public:
  /**
   * Constructor of a recorded Midi event
   * @param next - a pointer to the next Midi event
   * @param midi - the recorded Midi data.
   * @param timeStamp - the time point when the Midi event was recorded.
   */
  MidiEvent(FutureMidiEvent next, int midi, Std_time_point timeStamp);

  ~MidiEvent();

  /**
   * Get the future that waits for the Midi event following this Midi Event.
   *
   * This function passes the ownership of the next future midi event to the caller trough a `unique
   * pointer`. This means, this function can only be called once.
   * @return a unique pointer to the next future midi event.
   */
  FutureMidiEvent grabNext();

  int midi() const;
};

FutureMidiEvent launchNextFuture(int thisMidi) ;


} // namespace alsa_listener
#endif // A_J_MIDI_SRC_ALSA_LISTENER_H
