/*
 * File: alsa_receiver_queue.h
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
#ifndef A_J_MIDI_SRC_ALSA_RECEIVER_QUEUE_H
#define A_J_MIDI_SRC_ALSA_RECEIVER_QUEUE_H

#include <alsa/asoundlib.h>

#include <chrono>
#include <forward_list>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

/**
 *
 */
namespace alsaReceiverQueue {

class AlsaEvent;
using Sys_clock = std::chrono::steady_clock;
using Std_time_point = std::chrono::steady_clock::time_point;
/**
 * A smart pointer that owns and manages an AlsaEvent-object through a pointer and
 * disposes of that object when the AlsaEventPtr goes out of scope.
 */
using AlsaEventPtr = std::unique_ptr<AlsaEvent>;

/**
 * The state of the `alsaReceiverQueue`.
 */
enum class State : int {
  stopped,     /// the ReceiverQueue is stopped (initial state).
  running,     /// the ReceiverQueue is listening for incoming events.
 };

/**
 * The FutureAlsaEvent provides the mechanism to access the result
 * of asynchronously listen for incoming Alsa sequencer events.
 *
 * Once an event is received, the future is said to be ready.
 * and the received sequencer-event can be retrieved through the
 * function FutureAlsaEvent::get().
 *
 * When a future gets ready it recursively starts the
 * next FutureAlsaEvent.
 */
using FutureAlsaEvent = std::future<AlsaEventPtr>;

/**
 * A container that can hold several sequencer events.
 */
using EventContainer = std::forward_list<snd_seq_event_t>;

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
 * Start listening for incoming ALSA events.
 * A new FutureAlsaEvent is created.
 * The newly created future will be listening to
 * new ALSA events.
 * @param hSequencer handle to the ALSA sequencer.
 * @return the created FutureAlsaEvent.
 */
[[nodiscard("if the return value is discarded the queue might show an undefined behaviour.")]]
FutureAlsaEvent start(snd_seq_t *hSequencer);

/**
 * Force all processes to stop listening for incoming events.
 *
 * This function blocks until all listening processes have
 * ceased.
 */
void stop();

/**
 * Indicates the state of the current `alsaReceiverQueue`.
 * This function might block when the queue is shutting down.
 * @return the state of the current `alsaReceiverQueue`.
 */
State getState();

/**
 * Indicates whether the given FutureAlsaEvent is ready to deliver a result.
 * @param futureAlsaEvent - a FutureMidiEvent that might be ready
 * @return true - if there is a result,
 *         false - if the future is still waiting for an incoming Midi event.
 */
bool isReady(const FutureAlsaEvent &futureAlsaEvent);

/**
 * Get the number of events currently stored in the queue.
 * @return the number of events in the queue.
 */
int getCurrentEventCount();

/**
 * The class AlsaEvent wraps the midi data and sequencer instructions
 * recorded at one precise point of time.
 *
 * It holds a pointer to the next FutureAlsaEvent, thus every AlsaEvent forms
 * the head of a queue of recorded `AlsaEvent`s.
 */
class AlsaEvent {
private:
  FutureAlsaEvent _next;
  EventContainer _eventContainer;
  const Std_time_point _timeStamp;

public:
  /**
   * Constructor of a recorded Alsa event
   * @param next - a pointer to the next Alsa event
   * @param eventContainer - the recorded Alsa sequencer data.
   * @param timeStamp - the time point when the Midi event was recorded.
   */
  AlsaEvent(FutureAlsaEvent next, EventContainer eventContainer, Std_time_point timeStamp);

  ~AlsaEvent();

  /**
   * Consume the next Future.
   *
   * The returned value points to the head of a queue of interleaved Futures and
   * Midi Events.
   *
   * This function passes the ownership of the next FutureAlsaEvent to the
   * caller by moving the pointer to the caller. This means, this function can only be
   * called once.
   *
   * @return a unique pointer to the next future midi event.
   */
  [[nodiscard("if the return value is discarded, it will be destroyed")]]
  FutureAlsaEvent  grabNext();

  /**
   * The midi data of this event.
   * @return
   */


}; // AlsaEvent

} // namespace alsaReceiverQueue
#endif // A_J_MIDI_SRC_ALSA_RECEIVER_QUEUE_H
