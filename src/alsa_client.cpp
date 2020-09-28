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

#include "spdlog/spdlog.h"
#include <alsa/asoundlib.h>
#include <stdexcept>

namespace alsaClient {

/**
 * Implementation specific stuff.
 */
inline namespace impl {

static int g_queueId;                             /// the ID-number of the ALSA input queue
static int g_portId;                              /// the ID-number of our ALSA input port
static snd_seq_t *g_sequencerHandle{nullptr};     /// handle to access the ALSA sequencer
static snd_midi_event_t *g_midiEventParserHandle; /// handle to access the ALSA MIDI parser
static const char *g_deviceName;                  /// name of this client
static int g_clientId{0};                         /// the client-number of this client
static State g_stateFlag{State::closed};          /// the current state of the alsaClient
static std::mutex g_stateAccessMutex;             /// protects g_stateFlag against race conditions.


/**
 * Error handling for ALSA functions.
 * ALSA function often return the error code as a negative result. This function
 * checks the result for negativity.
 * - if negative, it prints the error message and dies.
 * - if positive or zero it does nothing.
 * @param operation description of the operation that was attempted.
 * @param alsaResult possible error code from an ALSA call.
 */
static void checkAlsa(const char *operation, int alsaResult) {
  if (alsaResult < 0) {
    constexpr int buffSize = 256;
    char buffer[buffSize];
    snprintf(buffer, buffSize, "Cannot %s - %s", operation, snd_strerror(alsaResult));
    SPDLOG_ERROR(buffer);
    throw std::runtime_error(buffer);
  }
}

static void reportAlsaError(const char *operation, int alsaResult) {
  if (alsaResult < 0) {
    constexpr int buffSize = 256;
    char buffer[buffSize];
    snprintf(buffer, buffSize, "Cannot %s - %s", operation, snd_strerror(alsaResult));
    SPDLOG_ERROR(buffer);
  }
}

} // namespace impl

/**
 * Open the ALSA sequencer in non-blocking mode.
 */
void open(const std::string &deviceName) noexcept(false) {
  int err;
  // open sequencer (do we need a duplex stream?).
  err = snd_seq_open(&g_sequencerHandle, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  checkAlsa("open sequencer", err);

  // set our client's name
  err = snd_seq_set_client_name(g_sequencerHandle, deviceName.c_str());
  checkAlsa("snd_seq_set_client_name", err);

  g_clientId = snd_seq_client_id(g_sequencerHandle);
  SPDLOG_TRACE("alsaClient::openAlsaSequencer - client {} created.", g_clientId);
}

void close() noexcept {
  // verify that the listening queue is stopped for sure.
  //  if (g_carryOnFlag) {
  //    SPDLOG_CRITICAL(
  //        "AlsaHelper::closeAlsaSequencer - attempt to stop while listening threads still run.");
  //    throw std::runtime_error("Sequencer cannot be stopped while listening threads still run.");
  //  }

  // kind of dirty hack!!!
  // Even when "carryOnFlag==false" there is a small chance that the
  // "listenForEventsLoop" accesses the sequencer for one
  // last time. To avoid this, lets sleep for a while.
  // std::this_thread::sleep_for(std::chrono::milliseconds(SHUTDOWN_POLL_PERIOD_MS));

  SPDLOG_TRACE("AlsaHelper::closeAlsaSequencer - closing client {}.", g_clientId);
  int err = snd_seq_close(g_sequencerHandle);
  reportAlsaError("snd_seq_close", err);
}

} // namespace alsaClient
