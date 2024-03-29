/*
 * File: alsa_client.cpp
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
#include "alsa_client.h"
#include "alsa_receiver_queue.h"

#include "alsa_util.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <alsa/asoundlib.h>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>

namespace alsaClient {

/**
 * Implementation specific stuff.
 */
inline namespace impl {
static auto g_logger = spdlog::stdout_color_mt("alsa_client");
static auto g_connectionsLogger = spdlog::stdout_color_mt("alsa_client-connections");

static int g_portId{NULL_ID};                 ///< the ID-number of our ALSA input port
static snd_seq_t *g_sequencerHandle{nullptr}; ///< handle to access the ALSA sequencer
static snd_midi_event_t *g_midiEventParserHandle{
    nullptr};                            ///< handle to access the ALSA MIDI parser
static int g_clientId{NULL_ID};          ///< the client-number of this client
static State g_stateFlag{State::closed}; ///< the current state of the alsaClient
static std::mutex g_stateAccessMutex;    ///< protects g_stateFlag against race conditions.
static std::string g_connectTo;          ///< the name of a port we shall try to connect to

// this should be large enough to hold the largest MIDI message to be encoded by the
// AlsaMidiEventParser
constexpr int MAX_MIDI_EVENT_SIZE{16};

/**
 * The `g_onMonitorConnectionsHandler` is invoked on regular time intervals.
 */
OnMonitorConnectionsHandler g_onMonitorConnectionsHandler{nullptr};
PortID defaultConnectionsHandler(const std::string &connectTo, const PortID &connectedTillNow);

/**
 * Returns a string representation of the given state.
 * @param state - the state
 * @return a string representation of the given state
 */
std::string stateAsString(State state) {
  switch (state) {
  case State::closed:
    return "closed";
  case State::idle:
    return "idle";
  case State::running:
    return "running";
  }
  return "unknown";
}

PortID tryToConnect(const std::string &designation) {
  if (designation.empty()) {
    SPDLOG_LOGGER_TRACE(g_connectionsLogger, "no connection requested");
    return NULL_PORT_ID;
  }
  auto searchProfile = toProfile(SENDER_PORT, designation);
  PortID target = findPort(searchProfile, matcher);
  if (target == NULL_PORT_ID) {
    SPDLOG_LOGGER_TRACE(g_connectionsLogger, "search for port {} - unsuccessful", designation);
    return target;
  }

  int err = snd_seq_connect_from(g_sequencerHandle, g_portId, target.client, target.port);
  if (err) {
    // It might happen that the function `findPort` reports a non-existing device.
    // Attempting to connect such a device, will result in an "invalid argument error".
    // We report the problem and ignore it.
    ALSA_INFO_ERROR(err, "tryToConnect::snd_seq_connect_from");
    return NULL_PORT_ID;
  }
  SPDLOG_LOGGER_INFO(g_connectionsLogger, "Connected to port {}", designation);
  return target;
}

static std::atomic<bool> g_monitoringActive{false}; ///< when false, ConnectionMonitoring will end.

void stopConnectionMonitoring() {
  SPDLOG_LOGGER_TRACE(g_connectionsLogger, "stopConnectionMonitoring");
  g_monitoringActive = false;
}
void stopInternal() noexcept {
  stopConnectionMonitoring();
  alsaClient::receiverQueue::stop();
}
void monitorLoop() {
  PortID currentlyConnected{NULL_PORT_ID};
  while (g_monitoringActive) {
    if (g_onMonitorConnectionsHandler) {
      SPDLOG_LOGGER_TRACE(g_connectionsLogger,
                          "monitorLoop - calling handler "
                          "g_connectTo = \"{}\"",
                          g_connectTo);
      currentlyConnected = g_onMonitorConnectionsHandler(g_connectTo, currentlyConnected);
    }
    std::this_thread::sleep_for(MONITOR_INTERVAL);
  }
}

void activateConnectionMonitoring() {
  SPDLOG_LOGGER_TRACE(g_connectionsLogger, "activateConnectionMonitoring");
  g_monitoringActive = true;
  // create and start the monitoring thread.
  std::thread monitorThread(monitorLoop);

  // set the priority to the lowest possible level
  sched_param schParams;
  schParams.sched_priority = 1; // = lowest
  if (pthread_setschedparam(monitorThread.native_handle(), SCHED_RR, &schParams)) {
    SPDLOG_LOGGER_ERROR(g_connectionsLogger, "Failed to set Thread scheduling : {}",
                        std::strerror(errno));
  }

  // Separate the thread of execution from the `monitorThread` object,
  // allowing execution to continue once this function is excited.
  monitorThread.detach();
}

void activateInternal(a2jmidi::ClockPtr clock) {
  activateConnectionMonitoring();
  alsaClient::receiverQueue::start(g_sequencerHandle, std::move(clock));
}
int identifierStrToInt(const std::string &identifier) noexcept {
  try {
    return std::stoi(identifier);
  } catch (...) {
    return NULL_ID;
  }
}

std::string normalizedIdentifier(const std::string &identifier) noexcept {
  try {
    std::string noBlanks = std::regex_replace(identifier, std::regex{"\\s"}, "");
    std::string noSpecial = std::regex_replace(noBlanks, std::regex{"[^a-zA-Z0-9]"}, "_");
    return noSpecial;
  } catch (...) {
    return identifier; // an ugly result is better than no result at all.
  }
}

PortProfile toProfile(PortCaps caps, const std::string &designation) {
  PortProfile result;
  result.caps = caps;

  if (designation.empty()) {
    result.hasError = true;
    result.errorMessage << "Port-Identifier seems to be empty.";
    return result;
  }

  std::smatch matchResults;

  std::regex twoNamesSeparatedByColon{"^([^:]+):([^:]+)$"};
  regex_match(designation, matchResults, twoNamesSeparatedByColon);
  if (!matchResults.empty()) {
    result.hasColon = true;
    result.firstName = normalizedIdentifier(matchResults[1]);
    result.secondName = normalizedIdentifier(matchResults[2]);
    result.firstInt = identifierStrToInt(result.firstName);
    result.secondInt = identifierStrToInt(result.secondName);
    return result;
  }

  std::regex oneName{"^[^:]+$"};
  regex_match(designation, matchResults, oneName);
  if (!matchResults.empty()) {
    result.hasColon = false;
    result.firstName = normalizedIdentifier(matchResults[0]);
    result.secondName.clear();
    result.firstInt = identifierStrToInt(result.firstName);
    result.secondInt = NULL_ID;
    return result;
  }

  result.hasError = true;
  result.errorMessage << "Invalid Port-Identifier: " << designation;
  return result;
}

/**
 * The implementation of the MatchCallback function.
 * @param caps - the capabilities of the actual port.
 * @param port - the formal identity of he actual port.
 * @param clientName - the name of the client to which the actual port belongs.
 * @param portName - the name of the port.
 * @param requested - the profile of the requested port.
 * @return true if the actual port matches the requested profile, false otherwise.
 */
bool matcher(PortCaps caps, PortID port, const std::string &clientName, const std::string &portName,
             const PortProfile &requested) {
  if (!fulfills(caps, requested.caps)) {
    return false;
  }
  std::string normalClientName{normalizedIdentifier(clientName)};
  std::string normalPortName{normalizedIdentifier(portName)};

  if (requested.hasColon) {
    if (requested.firstInt == port.client) {
      if (requested.secondInt == port.port) {
        return true;
      }
      if (normalizedIdentifier(requested.secondName) == normalPortName) {
        return true;
      }
    }
    if (normalizedIdentifier(requested.firstName) == normalClientName) {
      if (normalizedIdentifier(requested.secondName) == normalPortName) {
        return true;
      }
      if (requested.secondInt == port.port) {
        return true;
      }
    }
  } else {
    if (normalizedIdentifier(requested.firstName) == normalPortName) {
      return true;
    }
  }

  return false;
}

/**
 * Search through all MIDI ports known to the ALSA sequencer.
 * @param requested - the profile describing the kind of searched port.
 * @param match - a function that tests whether the actual port fulfills the requests from the
 * profile.
 * @return the first port that fulfills the requests or `NULL_PORT_ID` when non found.
 */
PortID findPort(const PortProfile &requested, const MatchCallback &match) {
  if (requested.hasError) {
    return NULL_PORT_ID;
  }
  snd_seq_client_info_t *clientInfo;
  snd_seq_port_info_t *portInfo;
  snd_seq_client_info_alloca(&clientInfo);
  snd_seq_port_info_alloca(&portInfo);

  snd_seq_client_info_set_client(clientInfo, NULL_ID);
  while (snd_seq_query_next_client(g_sequencerHandle, clientInfo) >= 0) {
    int clientNr = snd_seq_client_info_get_client(clientInfo);
    std::string clientName{snd_seq_client_info_get_name(clientInfo)};
    snd_seq_port_info_set_client(portInfo, clientNr);
    snd_seq_port_info_set_port(portInfo, NULL_ID);
    while (snd_seq_query_next_port(g_sequencerHandle, portInfo) >= 0) {
      int portNr = snd_seq_port_info_get_port(portInfo);
      std::string portName{snd_seq_port_info_get_name(portInfo)};
      PortCaps caps = snd_seq_port_info_get_capability(portInfo);
      PortID portId{clientNr, portNr};

      if (match(caps, portId, clientName, portName, requested)) {
        return portId;
      }
    }
  }
  return NULL_PORT_ID;
}
/**
 * The not-synchronized version of `receiverPortGetConnections()`.
 * @return a list of the ports to which the ReceiverPort is connected. If no
 * port is currently connected or the ReceiverPort has not been created yet,
 * an empty list is returned.
 */
std::vector<PortID> receiverPortGetConnectionsInternal() {
  std::vector<PortID> result;

  snd_seq_addr_t thisAddr;
  thisAddr.client = g_clientId;
  thisAddr.port = g_portId;

  snd_seq_query_subscribe_t *subscriptionData;
  snd_seq_query_subscribe_alloca(&subscriptionData);
  snd_seq_query_subscribe_set_root(subscriptionData, &thisAddr);
  snd_seq_query_subscribe_set_type(subscriptionData, SND_SEQ_QUERY_SUBS_WRITE);
  snd_seq_query_subscribe_set_index(subscriptionData, 0);

  while (snd_seq_query_port_subscribers(g_sequencerHandle, subscriptionData) >= 0) {
    const auto *subscriberAddr = snd_seq_query_subscribe_get_addr(subscriptionData);
    result.emplace_back(subscriberAddr->client, subscriberAddr->port);
    snd_seq_query_subscribe_set_index(subscriptionData,
                                      snd_seq_query_subscribe_get_index(subscriptionData) + 1);
  }
  return result;
}

midi::Event parseAlsaEvent(const snd_seq_event_t &alsaEvent) {
  static const midi::Event emptyEvent{};
  unsigned char pMidiData[MAX_MIDI_EVENT_SIZE];
  long evLength =
      snd_midi_event_decode(g_midiEventParserHandle, pMidiData, MAX_MIDI_EVENT_SIZE, &alsaEvent);
  if (evLength <= 0) {
    if (evLength == -ENOENT) {
      // The sequencer event does not correspond to one or more MIDI messages.
      return emptyEvent; // that's OK ... just ignore
    }
    ALSA_ERROR(evLength, "snd_midi_event_decode");
    return emptyEvent;
  }

  midi::Event result(pMidiData, pMidiData + evLength);
  return result;
}

/**
 * Register a handler that shall be called at regular time-intervals
 * to control the state of the connections to the port.
 * @param handler - the function to be called
 * @throws BadStateException - if the `alsaClient` is in `running` state.
 */
void onMonitorConnections(const OnMonitorConnectionsHandler &handler) {
  if (g_stateFlag == State::running) {
    throw BadStateException("Cannot register an OnMonitorConnectionsHandler. Wrong state " +
                            stateAsString(g_stateFlag));
  }
  g_onMonitorConnectionsHandler = handler;
}
PortID defaultConnectionsHandler(const std::string &connectTo, const PortID &connectedTillNow) {

  if (connectTo.empty()) {
    // connectTo is empty -> do nothing
    SPDLOG_LOGGER_TRACE(g_connectionsLogger, "ConnectionsHandler - no connection requested");
    return connectedTillNow;
  }
  if (g_portId == NULL_ID) {
    // oops, there is no port attached to this client (?) -> do nothing
    SPDLOG_LOGGER_TRACE(g_connectionsLogger, "ConnectionsHandler - no receiver port");
    return connectedTillNow;
  }

  if (connectedTillNow != NULL_PORT_ID) {
    // we had a connection. Verify whether it still is there...
    std::vector<PortID> connectedPorts = receiverPortGetConnectionsInternal();
    if (std::find(connectedPorts.begin(), //
                  connectedPorts.end(),   //
                  connectedTillNow) != connectedPorts.end()) {
      SPDLOG_LOGGER_TRACE(g_connectionsLogger, "ConnectionsHandler - connection still OK");
      return connectedTillNow;
    }
  }

  // let's try to connect to whatever "connectTo" might be.
  SPDLOG_LOGGER_TRACE(g_connectionsLogger, "check connections - trying to connect to {}",
                      connectTo);
  return tryToConnect(connectTo);
}
} // namespace impl

/**
 * Open the ALSA sequencer in non-blocking mode.
 */
void open(const std::string &clientName) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::closed) {
    throw BadStateException("Cannot open ALSA client. Wrong state " + stateAsString(g_stateFlag));
  }
  snd_seq_t *newSequencerHandle;
  snd_midi_event_t *newParserHandle;
  int err;
  // open sequencer (do we need a duplex stream?).
  err = snd_seq_open(&newSequencerHandle, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  if (ALSA_ERROR(err, "open sequencer")) {
    throw std::runtime_error("ALSA cannot open sequencer");
  }

  // set our client's name
  err = snd_seq_set_client_name(newSequencerHandle, clientName.c_str());
  if (ALSA_ERROR(err, "snd_seq_set_client_name")) {
    throw std::runtime_error("ALSA cannot set client name.");
  }

  // create the event parser
  err = snd_midi_event_new(MAX_MIDI_EVENT_SIZE, &newParserHandle);
  if (ALSA_ERROR(err, "snd_midi_event_new")) {
    throw std::runtime_error("ALSA cannot create MIDI parser.");
  }
  snd_midi_event_init(newParserHandle);
  snd_midi_event_no_status(newParserHandle, 1); // no running status byte!!!
  SPDLOG_LOGGER_TRACE(g_logger, "alsaClient::open - MIDI Event parser created.");

  // set common variables.
  g_portId = NULL_ID;
  g_sequencerHandle = newSequencerHandle;
  g_midiEventParserHandle = newParserHandle;
  g_clientId = snd_seq_client_id(g_sequencerHandle);
  if (ALSA_ERROR(g_clientId, "snd_seq_client_id")) {
    throw std::runtime_error("ALSA cannot create client");
  }
  g_stateFlag = State::idle;
  SPDLOG_LOGGER_TRACE(g_logger, "alsaClient::open - client {} created.", g_clientId);
}

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
                             const std::string &connectTo) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::idle) {
    throw BadStateException("Cannot create input port. Wrong state " + stateAsString(g_stateFlag));
  }
  if (g_portId != NULL_ID) {
    throw ServerException("Cannot create more that one port.");
  }
  g_portId = snd_seq_create_simple_port(g_sequencerHandle, portName.c_str(),
                                        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                        SND_SEQ_PORT_TYPE_APPLICATION);
  if (ALSA_ERROR(g_portId, "create port")) {
    g_portId = NULL_ID;
    throw std::runtime_error("ALSA cannot create port");
  }
  SPDLOG_LOGGER_TRACE(g_logger, "alsaClient::newInputAlsaPort - port \"{}\" created.", portName);

  g_connectTo = connectTo;
  onMonitorConnections(defaultConnectionsHandler);
}

/**
 * List all ports that are connected to the ReceiverPort.
 * @return a list of the ports to which the ReceiverPort is connected. If no
 * port is currently connected or the ReceiverPort has not been created yet,
 * an empty list is returned.
 */
std::vector<PortID> receiverPortGetConnections() {
  std::vector<PortID> emptyList{};
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed) {
    return emptyList;
  }
  if (g_portId == NULL_ID) {
    return emptyList;
  }
  return receiverPortGetConnectionsInternal();
}

void close() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed) {
    return;
  }
  // make sure that the input queue is stopped.
  stopInternal();

  SPDLOG_LOGGER_TRACE(g_logger, "alsaClient::closeAlsaSequencer - closing client {}.", g_clientId);
  snd_midi_event_free(g_midiEventParserHandle);
  int err = snd_seq_close(g_sequencerHandle);
  ALSA_ERROR(err, "close sequencer");

  // reset common variables to their null values.
  g_portId = NULL_ID;
  g_sequencerHandle = nullptr;
  g_midiEventParserHandle = nullptr;
  g_clientId = NULL_ID;
  g_stateFlag = State::closed;
}

std::string clientName() {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed) {
    return "";
  }
  snd_seq_client_info_t *info;
  snd_seq_client_info_alloca(&info);
  int err;

  err = snd_seq_get_client_info(g_sequencerHandle, info);
  if (ALSA_ERROR(err, "snd_seq_get_client_info")) {
    return "";
  }
  return snd_seq_client_info_get_name(info);
}
std::string portName() {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed) {
    return "";
  }
  if (g_portId == NULL_ID) {
    return "";
  }
  snd_seq_port_info_t *portInfo;
  snd_seq_port_info_alloca(&portInfo);

  int err = snd_seq_get_port_info(g_sequencerHandle, g_portId, portInfo);
  if (ALSA_ERROR(err, "snd_seq_get_port_info")) {
    return "";
  }
  return snd_seq_port_info_get_name(portInfo);
}
/**
 * Indicates the current state of the `alsaClient`.
 *
 * This function will block while the client is shutting down or starting up.
 * @return the current state of the `alsaClient`.
 */
State state() {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  return g_stateFlag;
}
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
void activate(a2jmidi::ClockPtr clock) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::idle) {
    throw BadStateException("Cannot create activate. Wrong state " + stateAsString(g_stateFlag));
  }
  if (!clock) {
    throw std::runtime_error("Clock pointer empty.");
  }
  activateInternal(std::move(clock));
  g_stateFlag = State::running;
  // make sure that the port monitor runs at least once.
  std::this_thread::sleep_for(MONITOR_INTERVAL);
}

void stop() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::running) {
    return;
  }
  stopInternal();
  g_stateFlag = State::idle;
}

int retrieve(const a2jmidi::TimePoint deadline, const RetrieveCallback &forEachClosure) noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::running) {
    return -1;
  }

  int err = 0;

  // we define the procedure to be executed on each MIDI event in the queue
  auto processClosure = [&forEachClosure, &err](const snd_seq_event_t &event,
                                                a2jmidi::TimePoint timeStamp) {
    const midi::Event midiEvent = parseAlsaEvent(event);
    if (!midiEvent.empty() && !err) {
      // we delegate to the given forEachClosure
      err = forEachClosure(midiEvent, timeStamp);
    }
  };
  // apply the processClosure on the queue
  alsaClient::receiverQueue::process(deadline, processClosure);
  return err;
}

} // namespace alsaClient
