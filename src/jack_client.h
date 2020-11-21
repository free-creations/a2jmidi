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

#include "a2jmidi_clock.h"
#include "sys_clock.h"
#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <jack/jack.h>
#include <jack/types.h>
#include <stdexcept>

namespace jackClient {


/**
 * The state of the `jackClient`.
 */
enum class State : int {
  closed,  /// the jackClient is not connected to the Jack server (initial state).
  idle,    /// the jackClient is connected to the Jack server, but not running.
  running, /// the jackClient is processing.
};

/**
 * When a function is called on the wrong state, `jackClient` throws
 * the `BadStateException`.
 */
class BadStateException : public std::runtime_error {
public:
  BadStateException(const std::string &whatArg) : runtime_error(whatArg) {}
};

/**
 * Errors that emanate from the JACK server are thrown as `ServerException`.
 */
class ServerException : public std::runtime_error {
public:
  ServerException(const char *whatArg) : runtime_error(whatArg) {}
};
/**
 * When JACK server is not started, `jackClient` throws
 * the ServerNotRunningException.
 */
class ServerNotRunningException : public ServerException {
public:
  // ServerNotRunningException(const char *whatArg) : ServerException(whatArg) {}
  ServerNotRunningException() : ServerException("JACK server not running") {}
};

/**
 * Indicates the current state of the `jackClient`.
 *
 * This function will block while the client is shutting down or starting up.
 * @return the current state of the `jackClient`.
 */
State state();
/**
 * Create a new Clock that gets its timing from the JACK server.
 * @return a smart pointer holding the clock.
 * @throws BadStateException - if the `jackClient` is in `closed` state.
 */
a2jmidi::ClockPtr clock();

/**
 * Open an external client session with the JACK server.
 *
 * When this function succeeds the `jackClient` is in `idle` state.
 *
 * @param clientName - a desired name for this client.
 * The server may modify this name to create a unique variant, if needed.
 * @param startServer - if true, the client will try to start the JACK server when it is not
 * already running.
 * @throws BadStateException - if the `jackClient` is not in `closed` state.
 * @throws ServerNotRunningException - if the JACK server is not running.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
void open(const std::string &clientName, bool startServer = false) noexcept(false);

/**
 * The name given by the JACK server to this client (aka device).
 *
 * This is the name that will be displayed in tools such as `QjackCtl`.
 *
 * As long as the client is not connected to the server (jackClient in closed state),
 * an empty string will be returned.
 * @return the name of this client.
 */
std::string clientName() noexcept;
/**
 * In future, we might introduce a dedicated `OutputPort` class.
 */
using JackPort = jack_port_t *;

/**
 * Create a new JACK MIDI port. External applications can read from this port.
 *
 * __Note 1__: in the current implementation, __only one__ output port can be created.
 *
 * __Note 2__: in the current implementation, this function can only be called from the
 * `idle` state.
 *
 * @param portName  - a desired name for the new port.
 * The server may modify this name to create a unique variant, if needed.
 * @return the output port.
 * @throws BadStateException - if port creation is attempted from a state other than `idle`.
 * @throws ServerException - if the JACK server has encountered a problem.
 */
JackPort newSenderPort(const std::string& portName) noexcept(false);


/**
 * Tell the JACK server that the client is ready to process.
 *
 * The `activate` function can only be called from the `idle` state.
 * Once activation succeeds, the `jackClient` is in `running` state and
 * the `processCallback` function will be invoked on each _cycle_.
 *
 * @throws BadStateException - if activation is attempted from a state other than `connected`.
 * @throws ServerException - if the JACK server has encountered a problem.
 */
void activate() noexcept(false);

/**
 * Tell the Jack server to stop calling the `processCallback` function
 * and end the current _session_.
 *
 * This client will be removed from the process graph. All ports belonging to
 * this client are closed.
 *
 * After this function returns, the `jackClient` is back into the `idle` state.
 */
void stop() noexcept;

/**
 * Disconnect this client from the JACK server.
 *
 * After this function returns, the `jackClient` is back into the `closed` state.
 */
void close() noexcept;

/**
 * Prototype for the client supplied function that is called on every cycle.
 * @param nFrames - number of frames to process
 * @param deadLine - the point in time where events are not for this, but the next cycle.
 * @return 0 on success, a non-zero value otherwise. Returning a non-Zero value will stop
 * the client.
 */
using ProcessCallback = std::function<int(const int nFrames, const a2jmidi::TimePoint deadLine)>;
/**
 * Prototype for the client supplied function that will be called when the
 * server is ending abnormally.
 */
using OnServerAbendHandler = std::function<void()>;
/**
 * Tell the Jack server to call the given processCallback function on each cycle.
 *
 * `registerProcessCallback()` can only be called from the `idle` state.
 *
 * @param processCallback - the function to be called
 * @throws BadStateException - if this function is called from a state other than `idle`.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
void registerProcessCallback(const ProcessCallback &processCallback) noexcept(false);
/**
 * Register a handler that shall be called when the server is ending abnormally.
 * @param handler - the function to be called
 * @throws BadStateException - if this function is called from a state other than `idle`.
 */
void onServerAbend(const OnServerAbendHandler &handler) noexcept(false) ;

/**
 * Implementation specific stuff.
 */
inline namespace impl {

/** handle to the JACK server **/
extern std::atomic<jack_client_t *> g_jackClientHandle;
/**
 * The current sample rate in samples per second.
 * @return the current sample rate in samples per second.
 */
inline int sampleRate() { return jack_get_sample_rate(g_jackClientHandle); }
} // namespace impl
} // namespace jackClient

#endif // A_J_MIDI_SRC_JACK_CLIENT_H
