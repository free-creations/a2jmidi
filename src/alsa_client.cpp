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

static int g_portId{-1};                          /// the ID-number of our ALSA input port
static snd_seq_t *g_sequencerHandle{nullptr};     /// handle to access the ALSA sequencer
static snd_midi_event_t *g_midiEventParserHandle; /// handle to access the ALSA MIDI parser
static std::string g_deviceName;              /// name of this client
static int g_clientId{-1};                        /// the client-number of this client
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

void stopInternal() noexcept {}

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
  g_portId = -1;
  g_sequencerHandle = newSequencerHandle;
  g_midiEventParserHandle = nullptr;
  g_deviceName = deviceName;
  g_clientId = snd_seq_client_id(newSequencerHandle);
  g_stateFlag = State::idle;
  SPDLOG_TRACE("alsaClient::open - client {} created.", g_clientId);
}

void close() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed) {
    return;
  }
  // make sure that the input queue is stopped.
  stopInternal();

  SPDLOG_TRACE("AlsaHelper::closeAlsaSequencer - closing client {}.", g_clientId);
  int err = snd_seq_close(g_sequencerHandle);
  ALSA_ERROR(err, "close sequencer");

  // reset common variables to their null values.
  g_portId = -1;
  g_sequencerHandle = nullptr;
  g_midiEventParserHandle = nullptr;
  g_deviceName = "";
  g_clientId = -1;
  g_stateFlag = State::closed;
}

} // namespace alsaClient
