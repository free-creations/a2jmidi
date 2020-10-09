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

#include "alsa_util.h"
#include "spdlog/spdlog.h"
#include <alsa/asoundlib.h>
#include <stdexcept>

namespace alsaClient {

/**
 * Implementation specific stuff.
 */
inline namespace impl {

static int g_portId{NULL_ID};                     /// the ID-number of our ALSA input port
static snd_seq_t *g_sequencerHandle{nullptr};     /// handle to access the ALSA sequencer
static snd_midi_event_t *g_midiEventParserHandle; /// handle to access the ALSA MIDI parser
static std::string g_portName;                    /// name of the unique port of this client
static int g_clientId{NULL_ID};                   /// the client-number of this client
static State g_stateFlag{State::closed};          /// the current state of the alsaClient
static std::mutex g_stateAccessMutex;             /// protects g_stateFlag against race conditions.

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
}

bool connectToPort(int destClient, int destPort) {
  if (destClient == NULL_ID) {
    return true;
  }
  if (destPort == NULL_ID) {
    return true;
  }
  int err = snd_seq_connect_to(g_sequencerHandle, g_portId, destClient, destPort);
  return (err <= 0);
}

void stopInternal() noexcept {

  SPDLOG_CRITICAL("alsaClient::stopInternal - not implemented yet!!!!!!");
}

PortIdInterpretation dissectPortIdentifier(const std::string& identifier){
  PortIdInterpretation result;


  return result;
}


void findPort(){
  // see aconnect.print_port < do_search_port
}

} // namespace impl

/**
 * Open the ALSA sequencer in non-blocking mode.
 */
void open(const std::string &deviceName) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::closed) {
    throw BadStateException("Cannot open ALSA client. Wrong state " + stateAsString(g_stateFlag));
  }
  snd_seq_t *newSequencerHandle;
  int err;
  // open sequencer (do we need a duplex stream?).
  err = snd_seq_open(&newSequencerHandle, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  if (ALSA_ERROR(err, "open sequencer")) {
    throw std::runtime_error("ALSA cannot open sequencer");
  }

  // set our client's name
  err = snd_seq_set_client_name(newSequencerHandle, deviceName.c_str());
  if (ALSA_ERROR(err, "set client name")) {
    throw std::runtime_error("ALSA cannot set client name");
  }

  // set common variables.
  g_portId = NULL_ID;
  g_sequencerHandle = newSequencerHandle;
  g_midiEventParserHandle = nullptr;
  g_clientId = snd_seq_client_id(g_sequencerHandle);
  if (ALSA_ERROR(g_clientId, "snd_seq_client_id")) {
    throw std::runtime_error("ALSA cannot create client");
  }
  g_stateFlag = State::idle;
  SPDLOG_TRACE("alsaClient::open - client {} created.", g_clientId);
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
 * @param connectTo - the name of an output port that this port shall try to connect.
 * If the connection fails, the port is nevertheless created. An empty string denotes
 * that no connection shall be attempted.
 * @return the input port.
 * @throws BadStateException - if port creation is attempted from a state other than `idle`.
 * @throws ServerException - if the ALSA server has encountered a problem.
 */
ReceiverPort newReceiverPort(const std::string &portName, int destClient, int destPort) noexcept(false) {
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
  g_portName = portName;
  SPDLOG_TRACE("alsaClient::newInputAlsaPort - port \"{}\" created.", portName);

  bool success = connectToPort(destClient, destPort);
  if (!success) {
    SPDLOG_INFO("alsaClient::newInputAlsaPort - could not connect to port {}:{}", destClient,
                destPort);
  }
}

void close() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed) {
    return;
  }
  // make sure that the input queue is stopped.
  stopInternal();

  SPDLOG_TRACE("alsaClient::closeAlsaSequencer - closing client {}.", g_clientId);
  int err = snd_seq_close(g_sequencerHandle);
  ALSA_ERROR(err, "close sequencer");

  // reset common variables to their null values.
  g_portId = NULL_ID;
  g_sequencerHandle = nullptr;
  g_midiEventParserHandle = nullptr;
  g_portName = "";
  g_clientId = NULL_ID;
  g_stateFlag = State::closed;
}

std::string deviceName() {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed){
    return "";
  }
  snd_seq_client_info_t* info;
  snd_seq_client_info_alloca(&info);
  int err;

  err = snd_seq_get_client_info(g_sequencerHandle, info);
  if (ALSA_ERROR(err, "snd_seq_get_client_info")){
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

  int err = snd_seq_get_port_info(g_sequencerHandle,g_portId,portInfo);
  if (ALSA_ERROR(err, "snd_seq_get_port_info")){
    return "";
  }
  return snd_seq_port_info_get_name(portInfo);
}
} // namespace alsaClient
