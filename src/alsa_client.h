/*
 * File: alsa_client.h
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
#ifndef A_J_MIDI_SRC_ALSA_CLIENT_H
#define A_J_MIDI_SRC_ALSA_CLIENT_H

#include "sys_clock.h"
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace alsaClient {
/**
 * Implementation specific stuff.
 */
inline namespace impl {} // namespace impl

using MidiEvent = std::vector<unsigned char>;

/**
 * When a function is called on the wrong state, `alsaClient` throws
 * the `BadStateException`.
 */
class BadStateException : public std::runtime_error {
public:
  BadStateException(const std::string &whatArg) : runtime_error(whatArg) {}
};

/**
 * Errors that emanate from the ALSA server are thrown as `ServerException`.
 */
class ServerException : public std::runtime_error {
public:
  ServerException(const char *whatArg) : runtime_error(whatArg) {}
};

/**
 * The state of the `alsaClient`.
 */
enum class State : int {
  closed,  /// the alsaClient is not connected to the ALSA server (initial state).
  idle,    /// the alsaClient is connected to the ALSA server, but not running.
  running, /// the alsaClient is processing.
};
/**
 * Indicates the current state of the `alsaClient`.
 *
 * This function will block while the client is shutting down or starting up.
 * @return the current state of the `alsaClient`.
 */
State state();
/**
 * Open an external client session with the ALSA server.
 *
 * When this function succeeds the `alsaClient` is in `idle` state.
 *
 * @param deviceName - a desired name for this client.
 * The server may modify this name to create a unique variant, if needed.
 * @throws BadStateException - if the `jackClient` is not in `closed` state.
 * @throws ServerNotRunningException - if the JACK server is not running.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
void open(const std::string &deviceName) noexcept(false);
/**
 * In future, we might introduce a dedicated `InputPort` class.
 */
using InputPort = void;

/**
 * Create a new ALSA MIDI input port. External applications can write to this port.
 *
 * __Note 1__: in the current implementation, __only one__ input port can be created.
 *
 * __Note 2__: in the current implementation, this function shall only be called from the
 * `idle` state.
 *
 * @param portName  - a desired name for the new port.
 * The server may modify this name to create a unique variant, if needed.
 * @param connectTo - the name of an output port that this port shall try to connect.
 * If the connection fails, the port is nevertheless created. An empty string denotes
 * that no connection shall be attempted.
 * @return the input port.
 * @throws BadStateException - if port creation is attempted from a state other than `idle`.
 * @throws ServerException - if the ALSA server has encountered a problem.
 */
InputPort newInputPort(const std::string &portName,
                       const std::string &connectTo = "") noexcept(false);


/**
 * Tell the ALSA server that the client is ready to process.
 *
 * The `activate` function can only be called from the `idle` state.
 * Once activation succeeds, the `alsaClient` is in `running` state and
 * will listen for incoming MIDI events.
 *
 * @throws BadStateException - if activation is attempted from a state other than `connected`.
 * @throws ServerException - if the ALSA server has encountered a problem.
 */
void activate() noexcept(false);
/**
 * Tell the  ALSA server to stop listening for incoming events.
 *
 * After this function returns, the `alsaClient` is back into the `idle` state.
 */
void stop() noexcept;
/**
 * Disconnect this client from the ALSA server.
 *
 * After this function returns, the `alsaClient` is back into the `closed` state.
 */
void close() noexcept;

/**
 * The function type to be used in the `retrieve` call.
 * @param event - the current ALSA-sequencer-event.
 * @param timeStamp - the point in time when the event was recorded.
 */
using ProcessCallback = std::function<void(const MidiEvent &event, sysClock::TimePoint timeStamp)>;

/**
 * Retrieve all events that were registered up to a given deadline.
 *
 * Events received beyond the given deadline will not be processed.
 *
 * All processed events will be removed from the input queue (and from memory).
 *
 * @param deadline - the time limit beyond which events will remain in the queue.
 * @param closure - the function to execute on each Event. It must be of type `ProcessCallback`.
 */
void retrieve(sysClock::TimePoint deadline, const ProcessCallback &closure) noexcept;

} // namespace alsaClient

#endif // A_J_MIDI_SRC_ALSA_CLIENT_H
