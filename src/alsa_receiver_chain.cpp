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
#include "alsa_receiver_chain.h"
#include "spdlog/spdlog.h"

namespace alsaReceiverChain {

State stateFlag{State::stopped};
std::mutex stateFlagMutex;


std::atomic<int> debugMidiEventObjectCount{0};


void setState(State newState) {
  std::unique_lock<std::mutex> lock {stateFlagMutex};
  stateFlag = newState;
}

State getState() {
  std::unique_lock<std::mutex> lock {stateFlagMutex};
  return stateFlag;
}



bool setStateAboutToStop() {
  std::unique_lock<std::mutex> lock {stateFlagMutex};
  if (stateFlag == State::running) {
    stateFlag = State::aboutToStop;
    return true;
  }
  return false;
}

void stop(FutureAlsaEvent anchor){

  if (!setStateAboutToStop()) {
    return;
  }

  while(stateFlag == State::aboutToStop) {
    auto status = anchor.wait_for(std::chrono::milliseconds(100));
    if (status != std::future_status::ready) {
      throw std::runtime_error("Cannot stop the alsaReceiverChain");
    }
    try {
      auto pAlsaEvent = anchor.get();
      anchor = std::move(pAlsaEvent->grabNext());
    } catch (const alsaReceiverChain::InterruptedException &) {
      stateFlag = State::stopped;
    }
  }
}
void interruptWhenRequested() {
  std::unique_lock<std::mutex> lock {stateFlagMutex};
  spdlog::trace("alsaReceiverChain::interruptWhenRequested() - state {}",  stateFlag);
  if (stateFlag != State::running) {
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

AlsaEvent_ptr listenForMidi(int previousMidi) {

  interruptWhenRequested();

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  interruptWhenRequested();

  // this simulates the receipt of an event
  auto thisMidi = previousMidi + 1;
  spdlog::trace("alsaReceiverChain::listenForEventsLoop: midi received starting "
                "the next future. state {}", stateFlag);

  // immediately startNextFuture a future to listen for the next midi-event.
  FutureAlsaEvent nextFuture = startNextFuture(thisMidi);

  // pack the this midi-events data and the next future into a `MidiEvent`
  // container.
  auto *pMidiEvent =
      new AlsaEvent(std::move(nextFuture), thisMidi, Sys_clock::now());

  // pass ownership of the `MidiEvent` container to the caller trough a `unique
  // pointer`.
  return AlsaEvent_ptr(pMidiEvent);
}

FutureAlsaEvent startNextFuture(int port) {
  return std::async(std::launch::async,
                    [port]() -> AlsaEvent_ptr { return listenForMidi(port); });
}

FutureAlsaEvent start(int port) {
  std::unique_lock<std::mutex> lock {stateFlagMutex};
  if (stateFlag != State::stopped) {
    throw std::runtime_error("Cannot start the alsaReceiverChain");
  }
  stateFlag = State::running;
  return startNextFuture(port);
}


bool isReady(const FutureAlsaEvent &futureAlsaEvent) {
  auto status = futureAlsaEvent.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

} // namespace alsaReceiverChain