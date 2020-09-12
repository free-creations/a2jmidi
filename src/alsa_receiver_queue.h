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

#include "sys_clock.h"

#include <alsa/asoundlib.h>
#include <chrono>
#include <functional>
#include <future>
#include <stdexcept>

/**
 *
 */
namespace alsaReceiverQueue {

/**
 * The state of the `alsaReceiverQueue`.
 */
enum class State : int {
  stopped, /// the ReceiverQueue is stopped (initial state).
  running, /// the ReceiverQueue is listening for incoming events.
};

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
 * @param hSequencer handle to the ALSA sequencer.
 */
void start(snd_seq_t *hSequencer) noexcept(false);

/**
 * Force all processes to stop listening for incoming events.
 *
 * The queue will be emptied all recorded events will be removed from the queue (and from memory).
 *
 * This function blocks until all listening processes have ceased.
 */
void stop() noexcept;

/**
 * Indicates the current state of the `alsaReceiverQueue`.
 *
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
 * Get an estimate of the number of events currently stored in the queue.
 * @return the number of Batches (events received at the same moment) in the queue.
 */
int getCurrentEventBatchCount();

/**
 * The function type to be used in the `process` call.
 * @param event - the current ALSA-sequencer-event.
 * @param timeStamp - the point in time when the event was recorded.
 */
using processCallback =
    std::function<void(const snd_seq_event_t &event, sysClock::TimePoint timeStamp)>;

/**
 * The process method executes a provided closure once for each registered
 * ALSA-sequencer-event.
 *
 * Events received beyond a given deadline will not be processed.
 *
 * All processed events will be removed from the queue (and from memory).
 *
 * @param deadline - the time limit beyond which events will remain in the queue.
 * @param closure - the function to execute on each Event. It must be of type `processCallback`.
 */
void process(sysClock::TimePoint deadline, const processCallback &closure) noexcept;

} // namespace alsaReceiverQueue
#endif // A_J_MIDI_SRC_ALSA_RECEIVER_QUEUE_H
