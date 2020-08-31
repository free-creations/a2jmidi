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
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

/**
 *
 */
namespace alsaReceiverQueue {

class AlsaEventBatch;
using Sys_clock = std::chrono::steady_clock;
using TimePoint = std::chrono::steady_clock::time_point;
/**
 * A smart pointer that owns and manages an AlsaEventBatch-object through a pointer and
 * disposes of that object when the AlsaEventPtr goes out of scope.
 */
using AlsaEventPtr = std::unique_ptr<AlsaEventBatch>;

/**
 * The state of the `alsaReceiverQueue`.
 */
enum class State : int {
  stopped, /// the ReceiverQueue is stopped (initial state).
  running, /// the ReceiverQueue is listening for incoming events.
};

/**
 * The FutureAlsaEvents provides the mechanism to access the result
 * of asynchronously listen for incoming Alsa sequencer events.
 *
 * Once an event is received, the future is said to be ready.
 * and the received sequencer-event can be retrieved through the
 * function FutureAlsaEvents::get().
 *
 * When a future gets ready it recursively starts the
 * next FutureAlsaEvents.
 */
using FutureAlsaEvents = std::future<AlsaEventPtr>;

/**
 * A container that can hold several sequencer events.
 */
using EventList = std::forward_list<snd_seq_event_t>;

/**
 * When a listener process is forced to stop, it throws
 * the InterruptedException.
 */
class InterruptedException : public std::future_error {
public:
  InterruptedException() : std::future_error(std::future_errc::broken_promise){};
};

/**
 * Start listening for incoming ALSA events.
 * A new FutureAlsaEvents is created.
 * The newly created future will be listening to
 * new ALSA events.
 * @param hSequencer handle to the ALSA sequencer.
 */
void start(snd_seq_t *hSequencer) ;

/**
 * Force all processes to stop listening for incoming events.
 *
 * This function blocks until all listening processes have
 * ceased.
 */
void stop();

/**
 * Indicates the state of the current `alsaReceiverQueue`.
 * This function will block while the queue is shutting down or starting up.
 * @return the current state of the `alsaReceiverQueue`.
 */
State getState();

/**
 * Indicates whether the alsaReceiverQueue has received at least one event.
 * @return true - if there is a result,
 *         false - if the queue is still waiting for a first incoming event.
 */
bool hasResult();

/**
 * Indicates whether the given FutureAlsaEvents is ready to deliver a result.
 * @return true - if there is a result,
 *         false - if the future is still waiting for an incoming Midi event.
 */
bool isReady(const FutureAlsaEvents &futureAlsaEvent);

/**
 * Get the number of events currently stored in the queue.
 * @return the number of event Batches in the queue.
 */
int getCurrentEventBatchCount();

/**
 * The function type to be used in the `process` call.
 * @param event - the current ALSA-sequencer-event.
 * @param timeStamp - the point in time when the event was recorded.
 */
using processCallback = std::function<void(const snd_seq_event_t &event, TimePoint timeStamp)>;

/**
 * The processInternal() method executes a provided function once for each
 * ALSA-sequencer-event recorded up to a given moment.
 *
 * All processed events will be removed from the queue.
 *
 * @param last - the time limit beyond which events will remain in the queue.
 * @param closure - the function to execute on each Event. It must be of type `processCallback`.
 */
void process( TimePoint last, const processCallback &closure) ;

} // namespace alsaReceiverQueue
#endif // A_J_MIDI_SRC_ALSA_RECEIVER_QUEUE_H
