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

namespace alsa_listener {

std::atomic<bool> carryOnListeningFlag{true};

inline void terminateListening() {
  carryOnListeningFlag = false;
}

/**
 * Indicates whether the listener processes shall carry on waiting for incoming Midi events.
 * @return true if the listener processes shall carry on,
 *         false if the listener processes shall stop.
 */
inline bool carryOnListening() {
  return carryOnListeningFlag;
}

MidiEvent::MidiEvent(FutureMidiEvent next, int midi, Std_time_point timeStamp)
    : _next{std::move(next)}, _midiValue{midi}, _timeStamp{timeStamp} {
  std::cout << "--- Result constructor " << _midiValue << std::endl;
}

MidiEvent::~MidiEvent() {
  std::cout << "--- Result destructor " << _midiValue << std::endl;
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
  std::cout << "--- Midi received  " << thisMidi << std::endl;

  // immediately go listening for the next midi event.
  FutureMidiEvent nextFuture = launchNextFuture(thisMidi);

  // pack the this midi and the future into a `Result` data structure.
  auto *result = new MidiEvent(std::move(nextFuture), thisMidi, Sys_clock::now());

  // pass ownership of `Result` data structure to the caller trough a `unique
  // pointer`.
  return MidiEvent_ptr(result);
}

FutureMidiEvent launchNextFuture(int thisMidi) {
  return std::async(std::launch::async, [thisMidi]() -> MidiEvent_ptr {
    return listenForMidi(thisMidi);
  });
}

} // namespace alsa_listener