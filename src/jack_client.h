/*
 * File: jack_client.h
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
#ifndef A_J_MIDI_SRC_JACK_CLIENT_H
#define A_J_MIDI_SRC_JACK_CLIENT_H

#include "sys_clock.h"
#include <functional>
#include <jack/types.h>
#include <stdexcept>
#include <iostream>

namespace jackClient {

/**
 * The state of the `jackClient`.
 */
enum class State : int {
  stopped,   /// the jackClient is stopped (initial state).
  connected, /// the jackClient is connected to the Jack server
  running,   /// the jackClient is listening for incoming events.
};

/**
 * When a function is called on the wrong state `jackClient` throws
 * the BadStateException.
 */
class BadStateException : public std::runtime_error {
public:
  BadStateException(const std::string& whatArg): runtime_error(whatArg) {}

};

/**
 * When a function is called on the wrong state `jackClient` throws
 * the ServerException.
 */
class ServerException : public std::runtime_error {
public:
  ServerException( const char* whatArg ): runtime_error(whatArg) {}
};
/**
 * When a function is called on the wrong state `jackClient` throws
 * the ServerNotRunningException.
 */
class ServerNotRunningException : public ServerException {
public:
  ServerNotRunningException( const char* whatArg ): ServerException(whatArg) {}
  ServerNotRunningException( ): ServerException("JACK server not running") {}
};




/**
 * Indicates the current state of the `jackClient`.
 *
 * This function will block while the queue is shutting down or starting up.
 * @return the current state of the `alsaReceiverQueue`.
 */
State state();


/**
 * Open an external client session with the JACK server.
 *
 * When this function succeeds the `jackClient` is in `connected` state.
 *
 * @param clientName - a desired name for this client.
 * The server may modify this name to create a unique variant, if needed.
 * @throws BadStateException - if the `jackClient` is not in `stopped` state.
 * @throws ServerNotRunningException - if the JACK server is not running.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
void open(const char *clientName) noexcept(false);


/**
 * The name that of this client.
 * @return the name that of this client.
 */
std::string clientName() noexcept;

/**
 * The handle to access the JACK server.
 * @return the handle to access the JACK server.
 * @throws BadStateException - if the `jackClient` is not in `connected` or `running` state.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
//jack_client_t *clientHandle() noexcept(false);

/**
 * Create a new JACK MIDI port. External applications can read from this port.
 * @param portName - a desired name for the new port.
 * The server may modify this name to create a unique variant, if needed.
 * @return a handle to the new port.
 * @throws BadStateException - if the `jackClient` is not in `connected` or `running` state.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
jack_port_t *newOutputMidiPort(const char *portName);

/**
 * Tell the JACK server that the client is ready to start processing.
 * The processCallback function will be invoked on each cycle.
 *
 * This function can only be called from the `connected` state.
 * After this function succeeds, the `jackClient` is in `running` state.
 *
 * @throws BadStateException - if this function is called on a state other than `connected`.
 * @throws ServerException - if the JACK server has encountered a problem.
 */
void activate() noexcept(false);

/**
 * Tell the Jack server to stop calling the processCallback function.
 * This client will be removed from the process graph. All ports belonging to
 * this client are closed.
 *
 * After this function returns, the `jackClient` is back into the `connected` state.
 */
void stop() noexcept;

/**
 * Disconnect this client from the JACK server.
 *
 * After this function returns, the `jackClient` is back into the `stopped` state.
 */
void close() noexcept;

/**
 * Prototype for the client supplied function that is called on every cycle.
 * @param nFrames - number of frames to process
 * @param deadLine - the point in time where events are not for this but the next cycle.
 * @return 0 on success, a non-zero value otherwise. Returning a non-Zero value will stop
 * the client.
 */
using ProcessCallback = std::function<int(int nFrames, sysClock::TimePoint deadLine)>;

/**
 * Tell the Jack server to call the given processCallback function on each cycle.
 *
 * `registerProcessCallback()` can only be called from the `connected` state.
 *
 * @param processCallback - the function to be called
 * @throws BadStateException - if this function is called from a state other than `connected`.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
void registerProcessCallback(const ProcessCallback &processCallback) noexcept(false);

} // namespace jackClient

#endif // A_J_MIDI_SRC_JACK_CLIENT_H
