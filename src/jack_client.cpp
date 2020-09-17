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
#include <mutex>
namespace jackClient {
inline namespace impl {
jack_client_t *g_hJackClient = nullptr;
} // namespace impl
/**
 * Protects the jackClient from being simultaneously accessed by multiple threads
 * while the state might change.
 */
static std::mutex g_stateAccessMutex;

static State g_stateFlag{State::stopped};

State stateInternal() { return g_stateFlag; }
/**
 * Indicates the current state of the `jackClient`.
 *
 * This function will block while the queue is shutting down or starting up.
 * @return the current state of the `alsaReceiverQueue`.
 */
State state() {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
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
  if (g_stateFlag == State::stopped) {
    return std::string("");
  }
  const char *actualClientName = jack_get_client_name(g_hJackClient);
  return std::string(actualClientName);
}
/**
 * The name that of this client.
 * @return the name that of this client.
 */
std::string clientName() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
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
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  SPDLOG_TRACE("jackClient::open");

  if (g_stateFlag != State::stopped) {
    throw BadStateException("Cannot open JACK client. Wrong state " + stateAsString(g_stateFlag));
  }

  // suppress jack error messages
  jack_set_error_function(jackErrorCallback);

  jack_status_t status;
  g_hJackClient = jack_client_open(clientName, JackNoStartServer, &status);
  if (!g_hJackClient) {
    SPDLOG_ERROR("Error opening JACK status={}.", status);
    throw ServerNotRunningException();
  }

  SPDLOG_TRACE("jackClient::open - success, status = {}", status);
  g_stateFlag = State::connected;
}

void stopInternal() {
  switch (g_stateFlag) {
  case State::stopped:
  case State::connected:
    return;
  case State::running: {
    if (g_hJackClient) {
      SPDLOG_TRACE("jackClient::stopInternal - stopping \"{}\".", clientNameInternal());
      int err = jack_deactivate(g_hJackClient);
      if (err) {
        SPDLOG_ERROR("jackClient::stopInternal - Error({})", err);
      }
    }
  }
  }
  g_stateFlag = State::connected;
}
/**
 * Tell the Jack server to stop calling the processCallback function.
 * This client will be removed from the process graph. All ports belonging to
 * this client are closed.
 *
 * After this function returns, the `jackClient` is back into the `connected` state.
 */
void stop() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  stopInternal();
}

/**
 * Disconnect this client from the JACK server.
 *
 * After this function has returned, the `jackClient` is back into the `stopped` state.
 */
void close() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::stopped) {
    return;
  }
  stopInternal();

  if (g_hJackClient) {
    SPDLOG_TRACE("jackClient::stopInternal - closing \"{}\".", clientNameInternal());
    int err = jack_client_close(g_hJackClient);
    if (err) {
      SPDLOG_ERROR("jackClient::close - Error({})", err);
    }
  }

  g_hJackClient = nullptr;
  g_stateFlag = State::stopped;
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
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::connected) {
    throw BadStateException("Cannot activate JACK client. Wrong state " +
                            stateAsString(g_stateFlag));
  }

  int err = jack_activate(g_hJackClient);
  if (err) {
    throw ServerException("Failed to activate JACK client!");
  }

  g_stateFlag = State::running;
}
static auto g_cycleLength = sysClock::SysTimeUnits();
static auto g_previousDeadline = sysClock::TimePoint();

sysClock::TimePoint resetTiming() {
  jack_nframes_t currentFrames; // JACKs frame time counter at the start of the current cycle
  jack_time_t currentUsecs;     // JACKs microseconds time at the start of the current cycle
  jack_time_t nextUsecs;        // JACKs microseconds time of the start of the next cycle
  float periodUsecs; //  JACKs current best estimate of the cycle-period time in microseconds.

  int err =
      jack_get_cycle_times(g_hJackClient, &currentFrames, &currentUsecs, &nextUsecs, &periodUsecs);
  if (err) {
    SPDLOG_CRITICAL("jackClient::resetTiming - error({})", err);
    return sysClock::now();
  }

  g_cycleLength = sysClock::toSysTimeUnits(periodUsecs);
  g_previousDeadline = sysClock::now();
  return g_previousDeadline;
}
bool isPlausible(sysClock::TimePoint deadline) {
  if (deadline >= sysClock::now()) {
    return false;
  }
  if (deadline < (sysClock::now() - g_cycleLength)) {
    return false;
  }
  return true;
}

sysClock::TimePoint newDeadline() {
  auto provisionalDeadline = g_previousDeadline + g_cycleLength;
  if (isPlausible(provisionalDeadline)) {
    g_previousDeadline = provisionalDeadline;
    return provisionalDeadline;
  }
  return resetTiming();
}

ProcessCallback g_customCallback = ProcessCallback();

/**
 * This callback will be invoked by the JACK server on each cycle.
 * It delegates to the custom defined callback.
 * @param nFrames - number of frames in one cycle
 * @param arg - (ignored) a pointer to an arbitrary data.
 * @return  0 on success, a non-zero value otherwise. Returning a non-Zero value will stop
 * the client.
 */
int jackInternalCallback(jack_nframes_t nFrames, void *arg) {
  if (g_customCallback) {
    return g_customCallback(nFrames, newDeadline());
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
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::connected) {
    throw BadStateException("Cannot register callback. Wrong state " + stateAsString(g_stateFlag));
  }
  g_customCallback = processCallback;
  int err = jack_set_process_callback(g_hJackClient, jackInternalCallback, nullptr);
  if (err) {
    throw ServerException("JACK error when registering callback.");
  }
}
} // namespace jackClient