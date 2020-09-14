/*
 * File: jack_client.cpp
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
#include "jack_client.h"
#include "spdlog/spdlog.h"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <mutex>
namespace jackClient {

static jack_client_t *hJackClient = nullptr; /// handle to the JACK server

/**
 * Protects the jackClient from being simultaneously accessed by multiple threads
 * while the state might change.
 */
static std::mutex stateAccessMutex;

static State stateFlag{State::stopped};

State stateInternal() { return stateFlag; }
/**
 * Indicates the current state of the `jackClient`.
 *
 * This function will block while the queue is shutting down or starting up.
 * @return the current state of the `alsaReceiverQueue`.
 */
State state() {
  std::unique_lock<std::mutex> lock{stateAccessMutex};
  return stateInternal();
}

/**
 *
 * @param state - the state
 * @return a string representation of the given state
 */
std::string stateAsString(State state) {
  switch (state) {
  case State::stopped:
    return "stopped";
  case State::connected:
    return "connected";
  case State::running:
    return "running";
  }
}

std::string clientNameInternal() noexcept {
  if (stateFlag == State::stopped) {
    return std::string("");
  }
  const char *actualClientName = jack_get_client_name(hJackClient);
  return std::string(actualClientName);
}
/**
 * The name that of this client.
 * @return the name that of this client.
 */
std::string clientName() noexcept {
  std::unique_lock<std::mutex> lock{stateAccessMutex};
  return clientNameInternal();
}

/**
 * we suppress all error messages from the JACK server.
 * @param msg - the message supplied by the server.
 */
void jackErrorCallback(const char *msg) { SPDLOG_INFO("jackClient::jackErrorCallback - {}", msg); }

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
void open(const char *clientName) noexcept(false) {
  std::unique_lock<std::mutex> lock{stateAccessMutex};
  SPDLOG_TRACE("jackClient::open");

  if (stateFlag != State::stopped) {
    throw BadStateException("Cannot open JACK client. Wrong state " + stateAsString(stateFlag));
  }

  // suppress jack error messages
  jack_set_error_function(jackErrorCallback);

  jack_status_t status;
  hJackClient = jack_client_open(clientName, JackNoStartServer, &status);
  if (!hJackClient) {
    SPDLOG_ERROR("Error opening JACK status={}.", status);
    throw ServerNotRunningException();
  }

  SPDLOG_TRACE("jackClient::open - success, status = {}", status);
  stateFlag = State::connected;
}

void stopInternal() {
  switch (stateFlag) {
  case State::stopped:
  case State::connected:
    return;
  case State::running: {
    if (hJackClient) {
      SPDLOG_TRACE("jackClient::stopInternal - stopping \"{}\".", clientNameInternal());
      int err = jack_deactivate(hJackClient);
      if (err) {
        SPDLOG_ERROR("jackClient::stopInternal - Error({})", err);
      }
    }
  }
  }
  stateFlag = State::connected;
}
/**
 * Tell the Jack server to stop calling the processCallback function.
 * This client will be removed from the process graph. All ports belonging to
 * this client are closed.
 *
 * After this function returns, the `jackClient` is back into the `connected` state.
 */
void stop() noexcept {
  std::unique_lock<std::mutex> lock{stateAccessMutex};
  stopInternal();
}

/**
 * Disconnect this client from the JACK server.
 *
 * After this function has returned, the `jackClient` is back into the `stopped` state.
 */
void close() noexcept {
  std::unique_lock<std::mutex> lock{stateAccessMutex};
  if (stateFlag == State::stopped) {
    return;
  }
  stopInternal();

  if (hJackClient) {
    SPDLOG_TRACE("jackClient::stopInternal - closing \"{}\".", clientNameInternal());
    int err = jack_client_close(hJackClient);
    if (err) {
      SPDLOG_ERROR("jackClient::close - Error({})", err);
    }
  }

  hJackClient = nullptr;
  stateFlag = State::stopped;
}
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
void activate() noexcept(false) {
  std::unique_lock<std::mutex> lock{stateAccessMutex};
  if (stateFlag != State::connected) {
    throw BadStateException("Cannot activate JACK client. Wrong state " + stateAsString(stateFlag));
  }

  int err = jack_activate(hJackClient);
  if (err) {
    throw ServerException("Failed to activate JACK client!");
  }

  stateFlag = State::running;
}

ProcessCallback customCallback = ProcessCallback();

int jackInternalCallback(jack_nframes_t cycleLength, void *arg) {
  if (customCallback) {
    return customCallback(cycleLength, sysClock::now());
  }
  return 0;
}

/**
 * Tell the Jack server to call the given processCallback function on each cycle.
 *
 * `registerProcessCallback()` can only be called from the `connected` state.
 *
 * @param processCallback - the function to be called
 * @throws BadStateException - if this function is called from a state other than `connected`.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
void registerProcessCallback(const ProcessCallback &processCallback) noexcept(false) {
  std::unique_lock<std::mutex> lock{stateAccessMutex};
  if (stateFlag != State::connected) {
    throw BadStateException("Cannot register callback. Wrong state " + stateAsString(stateFlag));
  }
  customCallback = processCallback;
  int err = jack_set_process_callback(hJackClient, jackInternalCallback, nullptr);
  if(err){
    throw ServerException("JACK error when registering callback.");
  }
}
} // namespace jackClient