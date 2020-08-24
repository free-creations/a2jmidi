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

namespace alsaReceiverQueue {

std::atomic<bool> shutdownFlag{false}; /// when true, the alsaReceiverQueue will be closed.
constexpr int SHUTDOWN_TIMEOUT_MS = 10; /// the time between two consecutive tests of shutdownFlag.

State stateFlag{State::stopped};
std::mutex stateFlagMutex;



std::atomic<int> currentEventCount{0};

/**
 * Get the number of events currently stored in the queue.
 * @return the number of events in the queue.
 */
int getCurrentEventCount(){
  return currentEventCount;
}


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
  SPDLOG_TRACE("alsaReceiverQueue::shutdown(), event-count {}, state {}", currentEventCount, stateFlag);
  // this will interrupt processing in "listenForEvent"
  shutdownFlag = true;
  // lets wait until all processes have polled the `shutdownFlag`
  std::this_thread::sleep_for(std::chrono::milliseconds(2*SHUTDOWN_TIMEOUT_MS));

  stateFlag= State::stopped;
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

void interruptWhenRequested() {
  if (shutdownFlag) {
    throw InterruptedException();
  }
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
  return std::move(_next); }

int AlsaEvent::midi() const { return _midiValue; }

FutureAlsaEvent startNextFuture(int port);

AlsaEvent_ptr listenForEvent(int previousMidi) {
  SPDLOG_TRACE("alsaReceiverQueue::listenForEvent");

  interruptWhenRequested();

  std::this_thread::sleep_for(std::chrono::milliseconds(SHUTDOWN_TIMEOUT_MS));
  interruptWhenRequested();

  // this simulates the receipt of an event
  auto thisMidi = previousMidi + 1;
  spdlog::trace("alsaReceiverQueue::listenForEventsLoop: midi received starting "
                "the next future. state {}",
                stateFlag);

  // immediately startNextFuture a future to listen for the next midi-event.
  FutureAlsaEvent nextFuture = startNextFuture(thisMidi);

  // pack the this midi-events data and the next future into a `MidiEvent`
  // container.
  auto *pMidiEvent = new AlsaEvent(std::move(nextFuture), thisMidi, Sys_clock::now());

  // pass ownership of the `MidiEvent` container to the caller trough a `unique
  // pointer`.
  return AlsaEvent_ptr(pMidiEvent);
}

FutureAlsaEvent startNextFuture(int port) {
  SPDLOG_TRACE("alsaReceiverQueue::startNextFuture");
  return std::async(std::launch::async, [port]() -> AlsaEvent_ptr { return listenForEvent(port); });
}

FutureAlsaEvent start(int port) {
  SPDLOG_TRACE("alsaReceiverQueue::start");
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  if (stateFlag == State::running) {
    shutdown();
    SPDLOG_ERROR("alsaReceiverQueue::start, attempt to start twice.");
    throw std::runtime_error("Cannot start the alsaReceiverQueue, it is already running.");
  }
  shutdownFlag = false;
  stateFlag = State::running;
  return startNextFuture(port);
}

bool isReady(const FutureAlsaEvent &futureAlsaEvent) {
  SPDLOG_TRACE("alsaReceiverQueue::isReady");
  auto status = futureAlsaEvent.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

} // namespace alsaReceiverQueue