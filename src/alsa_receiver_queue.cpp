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

static State stateFlag{State::stopped};
static std::mutex stateFlagMutex;

static FutureAlsaEvents queueHead{};

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
int getCurrentEventBatchCount() { return currentEventCount; }

/**
 * Indicates the state of the current `alsaReceiverQueue`.
 * This function might block when the queue is shutting down.
 * @return the state of the current `alsaReceiverQueue`.
 */
State getState() {
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  return stateFlag;
}

inline void invokeClosureForeachEvent(const EventContainer &eventsList, TimePoint current,
                                      const processCallback &closure) {
  for (const auto &event : eventsList) {
    closure(event, current);
  }
}



FutureAlsaEvents processInternal(FutureAlsaEvents &&queueHeadRemove, TimePoint last,
                         const processCallback &closure) {
  SPDLOG_TRACE("alsaReceiverQueue::processInternal() - event-count {}, state {}", currentEventCount,
               stateFlag);

  while (isReady(queueHeadRemove)) {
    try {
      AlsaEventPtr alsaEvents = queueHeadRemove.get(); // might throw when in shutdown mode.
      auto timestamp = alsaEvents->getTimeStamp();
      if (timestamp > last) {
        // the alsa events currently retrieved from the queue are premature.
        // to give them back for future retrieval, we must pack them into a
        // new FutureAlsaEvents object.
        std::promise<AlsaEventPtr> restartEvents;
        restartEvents.set_value(std::move(alsaEvents));
        return restartEvents.get_future();
      }
      invokeClosureForeachEvent(alsaEvents->getEventContainer(), timestamp, closure);
      queueHeadRemove = std::move(alsaEvents->grabNext());
    } catch (const InterruptedException &) {
      break;
    }
  }
  return std::move(queueHeadRemove);
}
void process( TimePoint last, const processCallback &closure) {
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  queueHead = std::move(processInternal(std::move(queueHead), last, closure));
}
/**
 * This is the not-synchronized version of `stop()`. It is used internally to avoid dead locks.
 */
void shutdown() {
  SPDLOG_TRACE("alsaReceiverQueue::shutdown(), event-count {}, state {}", currentEventCount,
               stateFlag);
  // this will interrupt processing in "listenForEvents".
  carryOnFlag = false;
  // lets wait until all processes have polled the `carryOnFlag`.
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * SHUTDOWN_POLL_PERIOD_MS));
  // remove all queued data.
  queueHead = std::move(FutureAlsaEvents{/*empty*/});

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
 * Constructor for a ALAS Events container.
 * @param next - a pointer to the next ALSA event
 * @param eventContainer - the recorded ALSA sequencer data.
 * @param timeStamp - the time point when the Midi event was recorded.
 */
AlsaEventBatch::AlsaEventBatch(FutureAlsaEvents next, EventContainer eventContainer, TimePoint timeStamp)
    : _next{std::move(next)}, _eventContainer{std::move(eventContainer)}, _timeStamp{timeStamp} {
  currentEventCount++;
  SPDLOG_TRACE("AlsaEventBatch::constructor, event-count {}, state {}", currentEventCount, stateFlag);
}

AlsaEventBatch::~AlsaEventBatch() {
  currentEventCount--;
  SPDLOG_TRACE("AlsaEventBatch::destructor, event-count {}, state {}", currentEventCount, stateFlag);
}

FutureAlsaEvents AlsaEventBatch::grabNext() {
  SPDLOG_TRACE("AlsaEventBatch::grabNext");
  return std::move(_next);
}

// forward declaration. Doc see below.
FutureAlsaEvents startNextFuture(snd_seq_t *hSequencer);

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
 * @return a smart pointer to an AlsaEventBatch object.
 */
AlsaEventPtr listenForEvents(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::listenForEvents");

  // poll descriptors for the poll function below.
  int fdsCount = snd_seq_poll_descriptors_count(hSequencer, POLLIN);
  checkAlsa("snd_seq_poll_descriptors_count", fdsCount);
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
        FutureAlsaEvents nextFuture = startNextFuture(hSequencer);

        // pack the the events data and the next future into an `AlsaEventBatch`- object.
        auto *pAlsaEvent = new AlsaEventBatch(std::move(nextFuture), events, Sys_clock::now());
        // delegate the ownership of the `AlsaEventBatch`-object to the caller by using a smart
        // pointer
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
 * @return an object of type `FutureAlsaEvents` that holds the future result.
 */
FutureAlsaEvents startNextFuture(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::startNextFuture");
  return std::async(std::launch::async,
                    [hSequencer]() -> AlsaEventPtr { return listenForEvents(hSequencer); });
}

/**
 * Start listening for incoming ALSA sequencer event.
 * A new FutureAlsaEvents is created.
 * The newly created future will be listening to
 * new ALSA sequencer events.
 * @param hSequencer handle to the ALSA sequencer.
 * @return the created FutureAlsaEvents.
 */
FutureAlsaEvents startInternal(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("alsaReceiverQueue::startInternal");
  //std::unique_lock<std::mutex> lock{stateFlagMutex};
  if (stateFlag == State::running) {
    shutdown();
    SPDLOG_ERROR("alsaReceiverQueue::start, attempt to startInternal twice.");
    throw std::runtime_error("Cannot startInternal the alsaReceiverQueue, it is already running.");
  }
  carryOnFlag = true;
  stateFlag = State::running;
  return startNextFuture(hSequencer);
}

void start(snd_seq_t *hSequencer) {
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  queueHead = std::move(startInternal(hSequencer));
}

bool isReady(const FutureAlsaEvents &futureAlsaEvent) {
  SPDLOG_TRACE("alsaReceiverQueue::isReady");
  if (!futureAlsaEvent.valid()) {
    return false;
  }
  // get status, waiting for the shortest possible time.
  auto status = futureAlsaEvent.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

bool hasResult() { return isReady(queueHead); }

} // namespace alsaReceiverQueue