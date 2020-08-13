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

/**
 *
 */
namespace alsa_listener {

class MidiEvent;
using Sys_clock = std::chrono::steady_clock;
using Std_time_point = std::chrono::steady_clock::time_point;
using MidiEvent_ptr = std::unique_ptr<MidiEvent>;

/**
 * A FutureMidiEvent is an object of type "std::future".
 * It is listening to a midi port until a new event is received.
 *
 * Once an event is received, the future is said to be ready.
 * and the received midi-event can be retrieved through the
 * function FutureMidiEvent::get().
 *
 * When a future gets ready it automatically starts the
 * next FutureMidiEvent.
 */
using FutureMidiEvent = std::future<MidiEvent_ptr>;
/**
 * Creates and starts a new FutureMidiEvent which is listening to
 * the given midi-port.
 * @param port an open midi input port
 * @return a unique pointer to the created FutureMidiEvent
 */
FutureMidiEvent startFuture(int port) ;

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
bool isReady(const FutureMidiEvent &futureMidiEvent) ;

/**
 * The class MidiEvent wraps the midi data received in one event and points to the
 * next even
 */
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

  /**
   * The midi data of this event.
   * @return
   */
  int midi() const;
};




} // namespace alsa_listener
#endif // A_J_MIDI_SRC_ALSA_LISTENER_H
