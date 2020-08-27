/*
 * File: alsa_receiver_queue.cpp
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
#include "alsa_receiver_queue.h"
#include "spdlog/spdlog.h"
#include <poll.h>

namespace alsaReceiverQueue {

std::atomic<bool> carryOnFlag{false}; /// when false, the alsaReceiverQueue will be closed.
constexpr int SHUTDOWN_POLL_PERIOD_MS =
    10; /// the time between two consecutive tests of carryOnFlag.

State stateFlag{State::stopped};
std::mutex stateFlagMutex;

/**
 * Error handling for ALSA functions.
 * ALSA function often return the error code as a negative result. This function
 * checks the result for negativity.
 * - if negative, it prints the error message and dies.
 * - if positive or zero it does nothing.
 * @param operation description of the operation that was attempted.
 * @param alsaResult possible error code from an ALSA call.
 * @ToDo move this function to a sequencer module.
 */
void checkAlsa(const char *operation, int alsaResult) {
  if (alsaResult < 0) {
    SPDLOG_CRITICAL("Cannot {} - {}", operation, snd_strerror(alsaResult));
    throw std::runtime_error("ALSA problem");
  }
}

std::atomic<int> currentEventCount{0}; /// the number of events currently stored in the queue.

/**
 * Get the number of events currently stored in the queue.
 * @return the number of events in the queue.
 */
int getCurrentEventCount() { return currentEventCount; }

/**
 * Indicates the state of the current `alsaReceiverQueue`.
 * This function might block when the queue is shutting down.
 * @return the state of the current `alsaReceiverQueue`.
 */
State getState() {
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  return stateFlag;
}

/**
 * This is the unsynchronized version of `stop()`. It is used internally to avoid dead locks.
 */
void shutdown() {
  SPDLOG_TRACE("alsaReceiverQueue::shutdown(), event-count {}, state {}", currentEventCount,
               stateFlag);
  // this will interrupt processing in "listenForEvents"
  carryOnFlag = false;
  // lets wait until all processes have polled the `carryOnFlag`
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * SHUTDOWN_POLL_PERIOD_MS));

  stateFlag = State::stopped;
}

/**
 * Force all processes to stop listening for incoming events.
 *
 * This function blocks until all listening processes have
 * ceased.
 */
void stop() {
  SPDLOG_TRACE("alsaReceiverQueue::stop, event-count {}, state {}", currentEventCount, stateFlag);
  // we lock access to the state flag during the full lockdown-time.
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  shutdown();
}

AlsaEvent::AlsaEvent(FutureAlsaEvent next, int midi, Std_time_point timeStamp)
    : _next{std::move(next)}, _midiValue{midi}, _timeStamp{timeStamp} {
  currentEventCount++;
  SPDLOG_TRACE("AlsaEvent::constructor, event-count {}, state {}", currentEventCount, stateFlag);
}

AlsaEvent::~AlsaEvent() {
  currentEventCount--;
  SPDLOG_TRACE("AlsaEvent::destructor, event-count {}, state {}", currentEventCount, stateFlag);
}

FutureAlsaEvent AlsaEvent::grabNext() {
  SPDLOG_TRACE("AlsaEvent::grabNext");
  return std::move(_next);
}

int AlsaEvent::midi() const { return _midiValue; }

FutureAlsaEvent startNextFuture(snd_seq_t *hSequencer);

int retrieveEvents(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::retrieveEvents");
  snd_seq_event_t *ev;
  int eventCount = 0;
  int status;

  do {
    status = snd_seq_event_input(hSequencer, &ev);
    switch (status) {
    case -EAGAIN:        // FIFO empty, lets deliver
      break;
    default:             //
      checkAlsa("snd_seq_event_input", status);
    }

    if (ev) {
      switch (ev->type) {
      case SND_SEQ_EVENT_NOTEON: //
        eventCount++;
        SPDLOG_TRACE("alsaReceiverQueue::retrieveEvents - got Event(Note on)");
        break;
      default: //
        SPDLOG_TRACE("alsaReceiverQueue::retrieveEvents -  got Event(other)");
      }
    }
  } while (status > 0);

  SPDLOG_TRACE("alsaReceiverQueue::retrieveEvents - eventCount {}.", eventCount);
  return eventCount;
}

AlsaEventPtr listenForEvents(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::listenForEvents");

  // create the poll descriptors that we will need when we wait for incoming events.
  int fdsCount = snd_seq_poll_descriptors_count(hSequencer, POLLIN);
  struct pollfd fds[fdsCount];

  while (carryOnFlag) {
    auto err = snd_seq_poll_descriptors(hSequencer, fds, fdsCount, POLLIN);
    checkAlsa("snd_seq_poll_descriptors", err);

    auto hasEvents = poll(fds, fdsCount, SHUTDOWN_POLL_PERIOD_MS);
    if (!carryOnFlag) {
      break;
    }
    if (hasEvents > 0) {
      auto events = retrieveEvents(hSequencer);

      // recursively call startNextFuture to listen for the next alsa-event.
      FutureAlsaEvent nextFuture = startNextFuture(hSequencer);

      // pack the the events data and the next future into a `AlsaEvent`
      // container.
      auto *pAlsaEvent = new AlsaEvent(std::move(nextFuture), events, Sys_clock::now());
      // move the ownership of the `MidiEvent`-container to the caller trough a `unique
      // pointer`.
      return AlsaEventPtr(pAlsaEvent);
    }
  }
  // got here, carryOnFlag is false. We have nothing to return - we throw InterruptedException
  throw InterruptedException();
}

FutureAlsaEvent startNextFuture(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::startNextFuture");
  return std::async(std::launch::async,
                    [hSequencer]() -> AlsaEventPtr { return listenForEvents(hSequencer); });
}

/**
 * Start listening for incoming ALSA events.
 * A new FutureAlsaEvent is created.
 * The newly created future will be listening to
 * new ALSA events.
 * @param hSequencer handle to the ALSA sequencer.
 * @return the created FutureAlsaEvent.
 */
FutureAlsaEvent start(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::start");
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  if (stateFlag == State::running) {
    shutdown();
    SPDLOG_ERROR("alsaReceiverQueue::start, attempt to start twice.");
    throw std::runtime_error("Cannot start the alsaReceiverQueue, it is already running.");
  }
  carryOnFlag = true;
  stateFlag = State::running;
  return startNextFuture(hSequencer);
}

bool isReady(const FutureAlsaEvent &futureAlsaEvent) {
  SPDLOG_TRACE("alsaReceiverQueue::isReady");
  auto status = futureAlsaEvent.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

} // namespace alsaReceiverQueue