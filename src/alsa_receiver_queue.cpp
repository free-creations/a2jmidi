/*
 * File: alsa_receiver_chain.cpp
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

std::atomic<bool> shutdownFlag{false}; /// when true, the alsaReceiverChain will be closed.
constexpr int SHUTDOWN_TIMEOUT_MS = 10; /// the time between two consecutive tests of shutdownFlag.

State stateFlag{State::stopped};
std::mutex stateFlagMutex;



std::atomic<int> debugMidiEventObjectCount{0};

/**
 * Indicates the state of the current `alsaReceiverChain`.
 * This function might block when the chain is shutting down.
 * @return the state of the current `alsaReceiverChain`.
 */
State getState() {
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  return stateFlag;
}

/**
 * Force all processes to stop listening for incoming events.
 *
 * All remaining `AlsaEvent` items will be removed from memory.
 * This function blocks until all listening processes have
 * ceased.
 *
 * @param queueHead the top (the oldest) element of the
 * current `alsaReceiverChain`.
 */
void stop(FutureAlsaEvent&&queueHead) {
  // we lock access to the state flag during the full lockdown-time.
  std::unique_lock<std::mutex> lock{stateFlagMutex};

  // this will interrupt processing in "listenForEvent"
  shutdownFlag = true;

  // lets go down the chain and search for the last (newest) item.
  while (stateFlag != State::stopped) {
    // make sure the current item becomes available in reasonable time.
    auto status = queueHead.wait_for(std::chrono::milliseconds(2*SHUTDOWN_TIMEOUT_MS));
    if (status != std::future_status::ready) {
      // item did not become available in a decent time... there is something very wrong.
      throw std::runtime_error("Cannot stop the alsaReceiverChain");
    }
    try {
      // get the result of the current item and switch to the next item.
      // The current item will be removed from memory.
      auto pAlsaEvent = queueHead.get();
      queueHead = std::move(pAlsaEvent->grabNext());
    } catch (const alsaReceiverQueue::InterruptedException &) {
      // OK ... `queueHead.get()` has thrown `InterruptedException`
      // we have reached the last (newest) item.
      stateFlag = State::stopped;
    }
  }
}
void interruptWhenRequested() {
  if (shutdownFlag) {
    throw InterruptedException();
  }
}

AlsaEvent::AlsaEvent(FutureAlsaEvent next, int midi, Std_time_point timeStamp)
    : _next{std::move(next)}, _midiValue{midi}, _timeStamp{timeStamp} {
  debugMidiEventObjectCount++;
  spdlog::trace("AlsaEvent::AlsaEvent count {}", debugMidiEventObjectCount);
}

AlsaEvent::~AlsaEvent() {
  debugMidiEventObjectCount--;
  spdlog::trace("AlsaEvent::~AlsaEvent count {}, state {}", debugMidiEventObjectCount, stateFlag);
}

FutureAlsaEvent AlsaEvent::grabNext() { return std::move(_next); }

int AlsaEvent::midi() const { return _midiValue; }

FutureAlsaEvent startNextFuture(int port);

AlsaEvent_ptr listenForEvent(int previousMidi) {

  interruptWhenRequested();

  std::this_thread::sleep_for(std::chrono::milliseconds(SHUTDOWN_TIMEOUT_MS));
  interruptWhenRequested();

  // this simulates the receipt of an event
  auto thisMidi = previousMidi + 1;
  spdlog::trace("alsaReceiverChain::listenForEventsLoop: midi received starting "
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
  return std::async(std::launch::async, [port]() -> AlsaEvent_ptr { return listenForEvent(port); });
}

FutureAlsaEvent start(int port) {
  std::unique_lock<std::mutex> lock{stateFlagMutex};
  if (stateFlag == State::running) {
    throw std::runtime_error("Cannot start the alsaReceiverChain, it is already running.");
  }
  shutdownFlag = false;
  stateFlag = State::running;
  return startNextFuture(port);
}

bool isReady(const FutureAlsaEvent &futureAlsaEvent) {
  auto status = futureAlsaEvent.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

} // namespace alsaReceiverQueue