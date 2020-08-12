/*
 * File: alsa_listener.h
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
#ifndef A_J_MIDI_SRC_ALSA_LISTENER_H
#define A_J_MIDI_SRC_ALSA_LISTENER_H

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace alsa_listener {

class InterruptedException : public std::future_error {
public:
  InterruptedException()
      : std::future_error(std::future_errc::broken_promise){};
};

std::atomic<bool> stop_listening{false};

class Result;

using Sys_clock = std::chrono::steady_clock;
using Std_time_point = std::chrono::steady_clock::time_point;
using Result_ptr = std::unique_ptr<Result>;
using Future_result = std::future<Result_ptr>;

inline bool resultReady(const Future_result &future) {
  auto status = future.wait_for(std::chrono::microseconds(0));
  return (status == std::future_status::ready);
}

class Result {
private:
  Future_result _next;
  const int _midiValue;
  const Std_time_point _timeStamp;

public:
  Result(Future_result next, int midi, Std_time_point timeStamp)
      : _next{std::move(next)}, _midiValue{midi}, _timeStamp{timeStamp} {
    std::cout << "--- Result constructor " << _midiValue << std::endl;
  };

  ~Result() {
    std::cout << "--- Result destructor " << _midiValue << std::endl;
  }

  Future_result grabNext() { return std::move(_next); }

  int midi() const { return _midiValue; }
};

Result_ptr listenForMidi(int previousMidi);

inline Future_result launchNextFuture(int thisMidi) {
  return std::async(std::launch::async, [thisMidi]() -> Result_ptr {
    return listenForMidi(thisMidi);
  });
}

inline Result_ptr listenForMidi(int previousMidi) {

  if (stopListening) {
    throw InterruptedException();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // this simulates the receipt of an event
  auto thisMidi = previousMidi + 1;
  std::cout << "--- Midi received  " << thisMidi << std::endl;

  // immediately go listening for the next midi event.
  Future_result nextFuture = launchNextFuture(thisMidi);

  // pack the this midi and the future into a `Result` data structure.
  auto *result = new Result(std::move(nextFuture), thisMidi, Sys_clock::now());

  // pass ownership of `Result` data structure to the caller trough a `unique
  // pointer`.
  return Result_ptr(result);
}

} // namespace alsa_listener
#endif // A_J_MIDI_SRC_ALSA_LISTENER_H
