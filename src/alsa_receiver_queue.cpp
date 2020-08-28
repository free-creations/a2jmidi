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

#include <utility>

namespace alsaReceiverQueue {

std::atomic<bool> carryOnFlag{false}; /// when false, the alsaReceiverQueue will be closed.
/**
 * the time in milliseconds between two consecutive tests of the carryOnFlag.
 */
constexpr int SHUTDOWN_POLL_PERIOD_MS = 10;

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

FutureAlsaEvent forEach(FutureAlsaEvent &&start, TimePoint last, const forEachCallback& closure) {

  while (isReady(start)) {
    try {
      auto pMidiEvent = start.get();
      snd_seq_event_t event;
      closure(event, Sys_clock::now());
      start = std::move(pMidiEvent->grabNext());
    } catch (const InterruptedException &) {
      break;
    }
  }
  return std::move(start);
}

/**
 * This is the not-synchronized version of `stop()`. It is used internally to avoid dead locks.
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

/**
 * Constructor of a recorded ALSA event
 * @param next - a pointer to the next ALSA event
 * @param eventContainer - the recorded ALSA sequencer data.
 * @param timeStamp - the time point when the Midi event was recorded.
 */
AlsaEvent::AlsaEvent(FutureAlsaEvent next, EventContainer eventContainer, TimePoint timeStamp)
    : _next{std::move(next)}, _eventContainer{std::move(eventContainer)}, _timeStamp{timeStamp} {
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

// forward declaration. Doc see below.
FutureAlsaEvent startNextFuture(snd_seq_t *hSequencer);

/**
 * Retrieve all events from the sequencers FIFO-queue.
 * @param hSequencer - a handle for the ALSA sequencer.
 * @return a set of sequencer events.
 */
EventContainer retrieveEvents(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::retrieveEvents");
  snd_seq_event_t *eventPtr;
  EventContainer eventContainer{};

  int status;

  do {
    status = snd_seq_event_input(hSequencer, &eventPtr);
    switch (status) {
    case -EAGAIN: // FIFO empty, lets deliver
      break;
    default: //
      checkAlsa("snd_seq_event_input", status);
    }
    if (eventPtr) {
      eventContainer.push_front(*eventPtr);
    }
  } while (status > 0);

  return eventContainer;
}

/**
 * Listen for one incoming event.
 *
 * Once an event is received, immediately a new thread is launched that will listen for
 * the next incoming event. The current thread returns normally.
 *
 * @param hSequencer - a handle for the ALSA sequencer.
 * @return a smart pointer to an AlsaEvent object.
 */
AlsaEventPtr listenForEvents(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::listenForEvents");

  // create the poll descriptors.
  int fdsCount = snd_seq_poll_descriptors_count(hSequencer, POLLIN);
  struct pollfd fds[fdsCount];

  while (carryOnFlag) {
    auto err = snd_seq_poll_descriptors(hSequencer, fds, fdsCount, POLLIN);
    checkAlsa("snd_seq_poll_descriptors", err);

    // wait until one or several incoming ALSA-sequencer-events are registered.
    auto hasEvents = poll(fds, fdsCount, SHUTDOWN_POLL_PERIOD_MS);
    if ((hasEvents > 0) && carryOnFlag) {
      auto events = retrieveEvents(hSequencer);
      if (!events.empty()) {
        // recursively call `startNextFuture()` to listen for the next ALSA sequencer event.
        FutureAlsaEvent nextFuture = startNextFuture(hSequencer);

        // pack the the events data and the next future into an `AlsaEvent`- object.
        auto *pAlsaEvent = new AlsaEvent(std::move(nextFuture), events, Sys_clock::now());
        // delegate the ownership of the `AlsaEvent`-object to the caller by using a smart pointer
        // ... and return (ending the current thread).
        return AlsaEventPtr(pAlsaEvent);
      }
    }
  }
  // Now `carryOnFlag` is false -> shutdown this thread
  throw InterruptedException();
}
/**
 * Launch a new thread that will be listening for the next ALSA sequencer event.
 * @param hSequencer - a handle for the ALSA sequencer.
 * @return an object of type `FutureAlsaEvent` that holds the future result.
 */
FutureAlsaEvent startNextFuture(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::startNextFuture");
  return std::async(std::launch::async,
                    [hSequencer]() -> AlsaEventPtr { return listenForEvents(hSequencer); });
}

/**
 * Start listening for incoming ALSA sequencer event.
 * A new FutureAlsaEvent is created.
 * The newly created future will be listening to
 * new ALSA sequencer events.
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