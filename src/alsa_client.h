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

#include "a2jmidi_clock.h"
#include "midi.h"
#include "sys_clock.h"
#include <alsa/asoundlib.h>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace alsaClient {

constexpr int NULL_ID = -1;
struct PortID {
public:
  const int client;
  const int port;
  constexpr PortID(int client, int port) : client{client}, port{port} {};

  bool operator==(const PortID &other) const {
    return ((other.port == port) && (other.client == client));
  }
  bool operator!=(const PortID &other) const {
    return ((other.port != port) || (other.client != client));
  }
};

constexpr PortID NULL_PORT_ID = PortID(NULL_ID, NULL_ID);

/**
 * Implementation specific stuff.
 */
inline namespace impl {

using namespace std::chrono_literals;

constexpr sysClock::SysTimeUnits MONITOR_INTERVAL{300ms};


using PortCaps = unsigned int;
/**
 * A _sender port_ has the capabilities to be __readable__ and to allow
 * read subscription. ALSA documentation calls such a port an __input__ port.
 */
constexpr PortCaps SENDER_PORT{SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ};
/**
 * A _receiver port_ has the capabilities to be __writable__ and to allow
 * write subscription. ALSA documentation calls such a port an __output__ port.
 */
constexpr PortCaps RECEIVER_PORT{SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ};
inline bool fulfills(PortCaps actualCaps, PortCaps requestedCaps){
  return (requestedCaps == (actualCaps & requestedCaps));
}
struct PortProfile {
public:
  PortProfile() = default;
  PortProfile(const PortProfile &) = delete; ///< no copy-constructor
  PortProfile(PortProfile &&) = default;     ///< move-constructor is defaulted
  bool hasError{false};                      ///< if true, a profile could not be established.
  std::stringstream errorMessage;            ///< a message to display if the profile is in error.
  PortCaps caps{SENDER_PORT};                ///< what kind of port are we searching for.
  bool hasColon{false};                      ///< are there two parts separated by colon?
  int firstInt{NULL_ID};  ///< if not NULL_ID -> the part before the colon is a valid integer
  std::string firstName;  ///< the part before the colon or the entire string if there was no colon.
  int secondInt{NULL_ID}; ///< if not NULL_ID -> the part after the colon is a valid integer
  std::string secondName; ///< the part after the colon could be this name
};

std::string normalizedIdentifier(const std::string &identifier) noexcept;

int identifierStrToInt(const std::string &identifier) noexcept;

PortProfile toProfile(PortCaps caps, const std::string &designation);
/**
 * Prototype for match function. Used in findPort.
 * @param caps - the capabilities of the actual port.
 * @param port - the formal identity of he actual port.
 * @param clientName - the name of the client to which the actual port belongs.
 * @param portName - the name of the port.
 * @param requested - the profile of the requested port.
 * @return true if the actual port matches the requested profile, false otherwise.
 */
using MatchCallback = std::function<bool(PortCaps caps, PortID port, const std::string &clientName,
                                         const std::string &portName, const PortProfile &requested)>;
/**
 * An implementation of the MatchCallback function.
 * @param caps - the capabilities of the actual port.
 * @param port - the formal identity of he actual port.
 * @param clientName - the name of the client to which the actual port belongs.
 * @param portName - the name of the port.
 * @param requested - the profile of the requested port.
 * @return true if the actual port matches the requested profile, false otherwise.
 */
bool match(PortCaps caps, PortID port, const std::string &clientName,
           const std::string &portName, const PortProfile &requested);

/**
 * Search through all MIDI ports known to the ALSA sequencer.
 * @param requested - the profile describing the searched port.
 * @param match - a function that returns true when the actual port fulfills the requests from the
 * profile.
 * @return the first port that fulfills the requests or `NULL_PORT_ID` when non found.
 */
PortID findPort(const PortProfile &requested, const MatchCallback &match);

/**
 * Prototype for the (system supplied) function that will be called in regular time
 * intervals to control the state of the connections to the port.
 * @param connectTo - the designation of a sender-port that the port shall try to connect.
 * An empty string denotes that no connection shall be attempted.
 */
using OnMonitorConnectionsHandler = std::function<void(const std::string &connectTo)>;

/**
 * Register a handler that shall be called be regular time-intervals
 * to control the state of the connections to the port.
 * @param handler - the function to be called
 * @throws BadStateException - if the `alsaClient` is in `running` state.
 */
void onMonitorConnections(const OnMonitorConnectionsHandler &handler) noexcept(false) ;


} // namespace impl

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
  closed,  ///< the alsaClient is not connected to the ALSA server (initial state).
  idle,    ///< the alsaClient is connected to the ALSA server, but not running.
  running, ///< the alsaClient is processing.
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
 * @param clientName - a desired name for this client.
 * The server may modify this name to create a unique variant, if needed.
 * @throws BadStateException - if the `alsaClient` is not in `closed` state.
 */
void open(const std::string &clientName) noexcept(false);
/**
 * In future, we might introduce a dedicated `ReceiverPort` class.
 */
using ReceiverPort = void;

/**
 * Create a new ALSA MIDI input port. External applications can write to this port.
 *
 * __Note 1__: in the current implementation, __only one single__ input port can be created.
 *
 * __Note 2__: in the current implementation, this function shall only be called from the
 * `idle` state.
 *
 * @param portName  - a desired name for the new port.
 * The server may modify this name to create a unique variant, if needed.
 * @param connectTo - the designation of a sender-port that this port shall try to connect.
 * If the connection fails, the port is nevertheless created. An empty string denotes
 * that no connection shall be attempted.
 * @return the input port.
 * @throws BadStateException - if port creation is attempted from a state other than `idle`.
 * @throws ServerException - if the ALSA server has encountered a problem.
 */
ReceiverPort newReceiverPort(const std::string &portName,
                             const std::string &connectTo = "") noexcept(false);

/**
 * List all ports that are connected to the ReceiverPort.
 * @return a list of the ports to which the ReceiverPort is connected. If no
 * port is currently connected or the ReceiverPort has not been created yet,
 * an empty list is returned.
 */
std::vector<PortID> receiverPortGetConnections();

/**
 * Tell the ALSA server that the client is ready to process.
 *
 * The `activate` function can only be called from the `idle` state.
 * Once activation succeeds, the `alsaClient` is in `running` state and
 * will listen for incoming MIDI events.
 * @param clock - the clock to be used to timestamp incoming events.
 * @throws BadStateException - if activation is attempted from a state other than `connected`.
 * @throws ServerException - if the ALSA server has encountered a problem.
 */
void activate(a2jmidi::ClockPtr clock) noexcept(false);
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
 * @param event - the current MIDI event.
 * @param timeStamp - the point in time when the event was recorded.
 * @return a non zero value if an error occurred.
 */
using RetrieveCallback =
    std::function<int(const midi::Event &event, const a2jmidi::TimePoint timeStamp)>;

/**
 * Retrieve all events that were registered up to a given deadline.
 *
 * Events received beyond the given deadline will not be processed.
 *
 * All processed events will be removed from the input queue (and from memory).
 *
 * @param deadline - the time limit beyond which events will remain in the queue.
 * @param forEachClosure - the function to execute on each Event. It must be of type `ProcessCallback`.
 * @return zero on success, a non zero value if an error occurred.
 */
int retrieve(a2jmidi::TimePoint deadline, const RetrieveCallback &forEachClosure) noexcept;
/**
 * The client-name aka device-name identifies a midi device or an application.
 * @return the name chosen by the ALSA system.
 */
std::string clientName();
/**
 *
 * @return the name of the port
 */
std::string portName();



} // namespace alsaClient

#endif // A_J_MIDI_SRC_ALSA_CLIENT_H
