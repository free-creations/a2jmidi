/*
 * File: alsa_listener.cpp
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
#include "alsa_listener.h"
#include "spdlog/spdlog.h"

namespace alsa_listener {

std::atomic<bool> carryOnListeningFlag{true};

void terminateListening() {
  carryOnListeningFlag = false;
}

/**
 * Indicates whether all the listener processes shall carry-on waiting for incoming Midi events.
 * @return true if the listener processes continue to listen,
 *         false if all listener processes shall stop as soon as possible.
 */
inline bool carryOnListening() {
  return carryOnListeningFlag;
}

MidiEvent::MidiEvent(FutureMidiEvent next, int midi, Std_time_point timeStamp)
    : _next{std::move(next)}, _midiValue{midi}, _timeStamp{timeStamp} {
  spdlog::trace("MidiEvent::MidiEvent count {}", 1);
}

MidiEvent::~MidiEvent() {
  spdlog::trace("MidiEvent::~MidiEvent count {}", 1);
}

FutureMidiEvent MidiEvent::grabNext() { return std::move(_next); }

int MidiEvent::midi() const { return _midiValue; }

MidiEvent_ptr listenForMidi(int previousMidi) {

  if (!carryOnListening()) {
    throw InterruptedException();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // this simulates the receipt of an event
  auto thisMidi = previousMidi + 1;
  spdlog::trace("alsa_listener::listenForMidi: midi received starting the next future");

  // immediately start a future to listen for the next midi-event.
  FutureMidiEvent nextFuture = startFuture(thisMidi);

  // pack the this midi-events data and the next future into a `MidiEvent` container.
  auto *pMidiEvent = new MidiEvent(std::move(nextFuture), thisMidi, Sys_clock::now());

  // pass ownership of the `MidiEvent` container to the caller trough a `unique
  // pointer`.
  return MidiEvent_ptr(pMidiEvent);
}

FutureMidiEvent startFuture(int port) {
  return std::async(std::launch::async, [port]() -> MidiEvent_ptr {
    return listenForMidi(port);
  });
}

bool isReady(const FutureMidiEvent &futureMidiEvent) {
  auto status = futureMidiEvent.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

} // namespace alsa_listener