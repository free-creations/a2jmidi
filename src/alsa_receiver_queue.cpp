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
#include <forward_list>
#include <memory>
#include <poll.h>
#include <utility>

namespace alsaClient::receiverQueue {

class AlsaEventBatch;
/**
 * A smart pointer that owns and manages an AlsaEventBatch-object through a pointer and
 * disposes of that object when the AlsaEventPtr goes out of scope.
 */
using AlsaEventPtr = std::unique_ptr<AlsaEventBatch>;
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

static std::atomic<bool> g_carryOnFlag{
    false}; /// when false, the receiverQueue will be shut down.
/**
 * the time in milliseconds between two consecutive tests of the carryOnFlag.
 */
constexpr int SHUTDOWN_POLL_PERIOD_MS = 10;

static State g_stateFlag{State::stopped};

/**
 * The number of event-batches currently stored in the queue.
 */
static std::atomic<int> g_currentEventBatchCount{0};
/**
 * The first (and oldest) element in the receiverQueue.
 */
static FutureAlsaEvents g_queueHead{};
/**
 * Protects the receiverQueue from being simultaneously accessed by multiple threads.
 */
static std::mutex g_queueAccessMutex;

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

/**
 * The class AlsaEventBatch wraps the midi data and sequencer instructions
 * recorded at one precise point of time.
 *
 * It holds a pointer to the next FutureAlsaEvents, thus every AlsaEventBatch forms
 * the head of a queue of recorded `AlsaEventBatch`s.
 */
struct AlsaEventBatch {
private:
  FutureAlsaEvents m_next;
  EventList m_eventList;
  const sysClock::TimePoint m_timeStamp;

public:
  /**
   * Constructor for an ALSA Events Batch container.
   * @param next - a pointer to the next ALSA event
   * @param eventList - the recorded ALSA sequencer data.
   * @param timeStamp - the time point when the events were recorded.
   */
  AlsaEventBatch(FutureAlsaEvents next, EventList eventList, sysClock::TimePoint timeStamp)
      : m_next{std::move(next)}, m_eventList{std::move(eventList)}, m_timeStamp{timeStamp} {
    g_currentEventBatchCount++;
    SPDLOG_TRACE("AlsaEventBatch::constructor, event-count {}, state {}", g_currentEventBatchCount,
                 g_stateFlag);
  }

  AlsaEventBatch(const AlsaEventBatch &other) = delete; // no copy constructor

  AlsaEventBatch &operator=(const AlsaEventBatch &other) = delete; // no copy assignment

  ~AlsaEventBatch() {
    g_currentEventBatchCount--;
    SPDLOG_TRACE("AlsaEventBatch::destructor, event-count {}, state {}", g_currentEventBatchCount,
                 g_stateFlag);
  }

  /**
   * Consume the next Future.
   *
   * The returned value points to the head of a queue of interleaved Futures and
   * Midi Events.
   *
   * This function passes the ownership of the next FutureAlsaEvents to the
   * caller by moving the pointer to the caller. This means, this function can only be
   * called once on a given AlsaEventBatch instance.
   *
   * @return a unique pointer to the next future midi event.
   */
  [[nodiscard]] FutureAlsaEvents grabNext() {
    SPDLOG_TRACE("AlsaEventBatch::grabNext");
    return std::move(m_next);
  }

  /**
   * Indicates the point in time when the events in this batch have been recorded.
   * @return the point in time when the events in this batch have been recorded.
   */
  sysClock::TimePoint getTimeStamp() { return m_timeStamp; }

  const EventList &getEventList() { return m_eventList; }
}; // AlsaEventBatch

/**
 * Get the number of events currently stored in the queue.
 * @return the number of events in the queue.
 */
int getCurrentEventBatchCount() { return g_currentEventBatchCount; }

/**
 * Indicates the state of the current `receiverQueue`.
 * This function might block when the queue is shutting down.
 * @return the state of the current `receiverQueue`.
 */
State getState() {
  std::unique_lock<std::mutex> lock{g_queueAccessMutex};
  return g_stateFlag;
}

inline void invokeClosureForeachEvent(const EventList &eventsList, sysClock::TimePoint current,
                                      const ProcessCallback &closure) {
  for (const auto &event : eventsList) {
    closure(event, current);
  }
}

/**
 * Indicates whether the given FutureAlsaEvents is ready to deliver a result.
 * @return true - if there is a result,
 *         false - if the future is still waiting for an incoming Midi event.
 */
bool isReady(const FutureAlsaEvents &futureAlsaEvent) {
  if (!futureAlsaEvent.valid()) {
    return false;
  }
  // get status, waiting for the shortest possible time.
  auto status = futureAlsaEvent.wait_for(std::chrono::microseconds(0));
  bool result = (status == std::future_status::ready);
  // SPDLOG_TRACE("receiverQueue::isReady - {}", result);
  return result;
}

FutureAlsaEvents processInternal(FutureAlsaEvents &&queueHeadInternal, sysClock::TimePoint deadline,
                                 const ProcessCallback &closure) {
//  SPDLOG_TRACE("receiverQueue::processInternal() - event-count {}, deadline {} us",
//                currentEventBatchCount,
//                std::chrono::duration<double,std::micro>(Sys_clock::now()-deadline).count());

  while (isReady(queueHeadInternal)) {
    try {
      AlsaEventPtr alsaEvents = queueHeadInternal.get(); // might throw when queue has been stopped.
      auto timestamp = alsaEvents->getTimeStamp();
      if (timestamp >= deadline) {
        // we have prematurely retrieved some AlsaEvents.
        // We must give them back. To this end, we will repack them
        // again into a FutureAlsaEvents object.
        std::promise<AlsaEventPtr> restartEvents;
        restartEvents.set_value(std::move(alsaEvents));
        return restartEvents.get_future();
      }
      invokeClosureForeachEvent(alsaEvents->getEventList(), timestamp, closure);
      queueHeadInternal = std::move(alsaEvents->grabNext());
    } catch (const InterruptedException &) {
      // we have tried to get an alsaEventPtr from a future that has been stopped.
      // So we will stop here.
      break;
    }
  }
  return std::move(queueHeadInternal);
}

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
void process(sysClock::TimePoint deadline, const ProcessCallback &closure) noexcept {
  std::unique_lock<std::mutex> lock{g_queueAccessMutex};
  if (g_queueHead.valid()) {
    g_queueHead = std::move(processInternal(std::move(g_queueHead), deadline, closure));
  }
}
/**
 * The not-synchronized version of `stop()`. It is used internally to avoid dead locks.
 */
void stopInternal() {
  SPDLOG_TRACE("receiverQueue::stopInternal(), event-count {}, state {}",
               g_currentEventBatchCount, g_stateFlag);
  // this will interrupt processing in "listenForEvents".
  g_carryOnFlag = false;
  // lets wait until all processes have polled the `carryOnFlag`.
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * SHUTDOWN_POLL_PERIOD_MS));
  // remove (delete from memory) all queued data.
  g_queueHead = std::move(FutureAlsaEvents{/*empty*/});

  g_stateFlag = State::stopped;
}

/**
 * Force all processes to stop listening for incoming events.
 *
 * This function blocks until all listening processes have
 * ceased.
 */
void stop() noexcept  {
  SPDLOG_TRACE("receiverQueue::stop, event-count {}, state {}", g_currentEventBatchCount,
               g_stateFlag);
  // we lock access to the queue during the full shutdown-time.
  std::unique_lock<std::mutex> lock{g_queueAccessMutex};
  stopInternal();
}

// forward declaration.
FutureAlsaEvents startNextFuture(snd_seq_t *hSequencer);

/**
 * Retrieve all events currently in the sequencers FIFO-queue.
 * @param hSequencer - a handle for the ALSA sequencer.
 * @return the list of sequencer events that were retrieved.
 */
EventList retrieveEvents(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("receiverQueue::retrieveEvents");
  snd_seq_event_t *eventPtr;
  EventList eventList{};
  int sequencerStatus;

  do {
    sequencerStatus = snd_seq_event_input(hSequencer, &eventPtr);
    switch (sequencerStatus) {
    case -EAGAIN: // sequencers FIFO is empty, return eventList.
      break;
    default: //
      checkAlsa("snd_seq_event_input", sequencerStatus);
    }
    if (eventPtr) {
      eventList.push_front(*eventPtr);
    }
  } while (sequencerStatus > 0);

  return eventList;
}

/**
 * This is the main listening loop which listens for a batch of incoming events.
 *
 * Once such a batch is received, immediately a new follow-on thread is launched.
 * This new thread will listen for the next incoming events and the current thread ends normally.
 *
 * If, while waiting, the `carryOnFlag` turns `false`, the current thread will end on
 * a `InterruptedException` and no follow-on thread will be launched.
 *
 * @param hSequencer - a handle for the ALSA sequencer.
 * @return a smart pointer to an AlsaEventBatch object which holds the received events and
 * the newly created future.
 */
AlsaEventPtr listenForEvents(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("receiverQueue::listenForEvents");

  // poll descriptors for the poll function below.
  int fdsCount = snd_seq_poll_descriptors_count(hSequencer, POLLIN);
  checkAlsa("snd_seq_poll_descriptors_count", fdsCount);
  struct pollfd fds[fdsCount];

  while (g_carryOnFlag) {
    auto err = snd_seq_poll_descriptors(hSequencer, fds, fdsCount, POLLIN);
    checkAlsa("snd_seq_poll_descriptors", err);

    // wait until one or several incoming ALSA-sequencer-events are registered.
    auto hasEvents = poll(fds, fdsCount, SHUTDOWN_POLL_PERIOD_MS);
    if ((hasEvents > 0) && g_carryOnFlag) {
      auto events = retrieveEvents(hSequencer);
      if (!events.empty()) {
        // recursively call `startNextFuture()` to listen for the next ALSA sequencer event.
        FutureAlsaEvents nextFuture = startNextFuture(hSequencer);

        // pack the the events data and the next future into an `AlsaEventBatch`- object.
        auto *pAlsaEvent = new AlsaEventBatch(std::move(nextFuture), events, sysClock::now());
        // delegate the ownership of the `AlsaEventBatch`-object to the caller by using a smart
        // pointer
        // ... and return (ending the current thread).
        return AlsaEventPtr(pAlsaEvent);
      }
    }
  }
  // Here `carryOnFlag` is false -> we end this thread on the appropriate exception.
  throw InterruptedException();
}
/**
 * Launch a new thread that will be listening for the next ALSA sequencer event.
 * @param hSequencer - a handle for the ALSA sequencer.
 * @return an object of type `FutureAlsaEvents` that holds the future result.
 */
FutureAlsaEvents startNextFuture(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("receiverQueue::startNextFuture");
  return std::async(std::launch::async,
                    [hSequencer]() -> AlsaEventPtr { return listenForEvents(hSequencer); });
}

/**
 * Internally called by `receiverQueue::start()`
 *
 * A new FutureAlsaEvents is created.
 * The newly created future will be listening to
 * new ALSA sequencer events.
 * @param hSequencer handle to the ALSA sequencer.
 * @return the newly created future.
 */
FutureAlsaEvents startInternal(snd_seq_t *hSequencer) {
  SPDLOG_TRACE("receiverQueue::startInternal");
  if (g_stateFlag == State::running) {
    stopInternal();
    SPDLOG_ERROR("receiverQueue::startInternal, attempt to start twice.");
    throw std::runtime_error("Cannot start the receiverQueue, it is already running.");
  }
  g_carryOnFlag = true;
  g_stateFlag = State::running;
  return startNextFuture(hSequencer);
}

/**
 * Start listening for incoming ALSA sequencer event.
 * @param hSequencer handle to the ALSA sequencer.
 */
void start(snd_seq_t *hSequencer) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_queueAccessMutex};
  g_queueHead = std::move(startInternal(hSequencer));
}

/**
 * Indicates whether the receiverQueue has received at least one event.
 * @return true - if there is a result,
 *         false - if the queue is still waiting for a first incoming event.
 */
bool hasResult() {
  std::unique_lock<std::mutex> lock{g_queueAccessMutex};
  return isReady(g_queueHead);
}

} // namespace alsaClient::receiverQueue