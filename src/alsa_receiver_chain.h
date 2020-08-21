/*
 * File: alsa_receiver_chain.h
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
#ifndef A_J_MIDI_SRC_ALSA_RECEIVER_CHAIN_H
#define A_J_MIDI_SRC_ALSA_RECEIVER_CHAIN_H

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

/**
 *
 */
namespace alsaReceiverChain {

class AlsaEvent;
using Sys_clock = std::chrono::steady_clock;
using Std_time_point = std::chrono::steady_clock::time_point;
using AlsaEvent_ptr = std::unique_ptr<AlsaEvent>;

/**
 * The state of the alsaReceiverChain.
 */
enum class State : int {
  stopped,     /// the ReceiverChain is stopped (initial state).
  running,     /// the ReceiverChain is listening for incoming events.
  aboutToStop, /// the ReceiverChain has been requested to stop.
};

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
using FutureAlsaEvent = std::future<AlsaEvent_ptr>;

/**
 * Creates and starts a new FutureMidiEvent which will be listening to
 * new alsa events.
 * @param port an open midi input port
 * @return a unique pointer to the created FutureMidiEvent
 */
FutureAlsaEvent startFuture(int port);

/**
 * When a listener process is forced to stop, it throws
 * the InterruptedException.
 */
class InterruptedException : public std::future_error {
public:
  InterruptedException()
      : std::future_error(std::future_errc::broken_promise){};
};

/**
 * Force all listeners to stop listening for incoming events.
 */
void stop();

/**
 * Indicates whether the listener processes shall carry on waiting for incoming
 * Midi events.
 * @return true - if the listener processes shall carry on,
 *         false - if the listener processes shall stop.
 */
State getState();

/**
 * Indicates whether the given future is ready to deliver a result.
 * @param futureAlsaEvent - a FutureMidiEvent that might be ready
 * @return true - if there is a result, false - if the future is still waiting
 * for an incoming Midi event.
 */
bool isReady(const FutureAlsaEvent &futureAlsaEvent);

/**
 * The class AlsaEvent wraps the midi data or sequencer instructions
 * received in one moment and points to the event received next.
 */
class AlsaEvent {
private:
  FutureAlsaEvent _next;
  const int _midiValue;
  const Std_time_point _timeStamp;

public:
  /**
   * Constructor of a recorded Midi event
   * @param next - a pointer to the next Midi event
   * @param midi - the recorded Midi data.
   * @param timeStamp - the time point when the Midi event was recorded.
   */
  AlsaEvent(FutureAlsaEvent next, int midi, Std_time_point timeStamp);

  ~AlsaEvent();

  /**
   * Consume the next Future.
   *
   * The returned value points to the head of a chain of interleaved Futures and
   * Midi Events.
   *
   * This function passes the ownership of the next future midi event to the
   * caller trough a `unique pointer`. This means, this function can only be
   * called once.
   *
   * @return a unique pointer to the next future midi event.
   */
  [[nodiscard("if the return value is discarded, it will be "
              "destroyed")]]
  FutureAlsaEvent  grabNext();

  /**
   * The midi data of this event.
   * @return
   */
  int midi() const;
};

} // namespace alsaReceiverChain
#endif // A_J_MIDI_SRC_ALSA_RECEIVER_CHAIN_H
