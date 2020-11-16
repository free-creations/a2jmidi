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
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <climits>
#include <mutex>
#include <thread>
namespace jackClient {
inline namespace impl {

/**
 * The logger for this file.
 */
static auto g_logger = spdlog::stdout_color_mt("jack_client");

std::atomic<jack_client_t *> g_jackClientHandle{nullptr};
/**
 * The 'g_customCallback' is invoked on each cycle.
 */
ProcessCallback g_customCallback{nullptr};

/**
 * The `g_onServerAbendHandler` is invoked on if the server ends abnormally.
 */
OnServerAbendHandler g_onServerAbendHandler{nullptr};

/**
 * Protects the jackClient from being simultaneously accessed by multiple threads
 * while the state might change.
 */
static std::mutex g_stateAccessMutex;

static State g_stateFlag{State::closed};

inline State stateInternal() { return g_stateFlag; }
/**
 * The JackClock is an instance of the general clock.
 * This class gets the time from the JACK sever.
 */
class JackClock : public a2jmidi::Clock {
public:
  /**
   * Destructor
   */
  ~JackClock() override = default;
  /**
   * The estimated current time in frames.
   * @return the estimated current time in system specific ticks.
   */
  long now() override {
    if (!g_jackClientHandle) {
      return LONG_MAX;
    }
    return jack_frame_time(g_jackClientHandle);
  }
};

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

std::string clientNameInternal() noexcept {
  if (g_stateFlag == State::closed) {
    return std::string("");
  }
  const char *actualClientName = jack_get_client_name(g_jackClientHandle);
  return std::string(actualClientName);
}

/**
 * we suppress all error messages from the JACK server.
 * @param msg - the message supplied by the server.
 */
void jackErrorCallback(const char *msg) {
  SPDLOG_LOGGER_ERROR(g_logger,
                      "jackClient::jackErrorCallback - "
                      "\n    --- message ---- \n"
                      "      {}"
                      "\n    --- message-end ---- \n",
                      msg);
}
/**
 * we suppress all info messages from the JACK server.
 * @param msg - the message supplied by the server.
 */
void jackInfoCallback(const char *msg) {
  SPDLOG_LOGGER_INFO(g_logger,
                     "jackClient::jackInfoCallback - "
                     "\n --- message ---- \n"
                     "{}"
                     "\n --- message-end ---- \n",
                     msg);
}

void stopInternal() {
  switch (g_stateFlag) {
  case State::closed:
  case State::idle:
    return; // do nothing if already stopped
  case State::running: {
    if (g_jackClientHandle) {
      SPDLOG_LOGGER_TRACE(g_logger, "jackClient::stopInternal - stopping \"{}\".",
                          clientNameInternal());
      int err = jack_deactivate(g_jackClientHandle);
      if (err) {
        SPDLOG_LOGGER_ERROR(g_logger, "jackClient::stopInternal - Error({})", err);
      }
    }
  }
  }
  g_onServerAbendHandler = nullptr;
  g_customCallback = nullptr;
  g_stateFlag = State::idle;
}


using namespace std::chrono_literals;



/**
 * @return the precise time at the start of the current process cycle.
 * This function may only be used from the process callback.
 */
inline a2jmidi::TimePoint newDeadline() {
  return jack_last_frame_time(g_jackClientHandle);
}

void jackShutdownCallback([[maybe_unused]] void *arg) {
  if (g_stateFlag == State::running) {
    if (g_onServerAbendHandler) {
      // execute the handler in its own thread.
      std::thread handlerThread(g_onServerAbendHandler);
      handlerThread.detach();
    }
  }
}

/**
 * This callback will be invoked by the JACK server on each cycle.
 * It delegates to the custom defined callback.
 * @param nFrames - number of frames in the current cycle
 * @param arg - (unused) a pointer to an arbitrary, user supplied, data.
 * @return  0 on success, a non-zero value otherwise. __Returning a non-Zero value will stop
 * the client__.
 */
int jackInternalCallback(jack_nframes_t nFrames, [[maybe_unused]] void *arg) {
  if (g_customCallback) {
    return g_customCallback(nFrames, newDeadline());
  }
  return 0;
}
} // namespace impl
/**
 * The name given by the JACK server to this client.
 * As long as the client is not connected to the server, an empty string will be returned.
 * @return the name of this client.
 */
std::string clientName() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  return clientNameInternal();
}
/**
 * Disconnect this client from the JACK server.
 *
 * After this function has returned, the `jackClient` is back into the `closed` state.
 */
void close() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag == State::closed) {
    return;
  }
  stopInternal();

  if (g_jackClientHandle) {
    SPDLOG_LOGGER_TRACE(g_logger, "jackClient::close - closing \"{}\".", clientNameInternal());
    int err = jack_client_close(g_jackClientHandle);
    if (err) {
      SPDLOG_LOGGER_ERROR(g_logger, "jackClient::close - Error({})", err);
    }
  }

  g_jackClientHandle = nullptr;
  g_stateFlag = State::closed;
}
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
void open(const std::string &clientName, bool startServer) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  SPDLOG_LOGGER_TRACE(g_logger, "jackClient::open");

  if (g_stateFlag != State::closed) {
    throw BadStateException("Cannot open JACK client. Wrong state " + stateAsString(g_stateFlag));
  }

  // suppress jack error messages
  jack_set_error_function(jackErrorCallback);
  jack_set_info_function(jackInfoCallback);

  jack_status_t status;
  JackOptions options = (startServer) ? JackNullOption : JackNoStartServer;
  g_jackClientHandle = jack_client_open(clientName.c_str(), options, &status);
  if (!g_jackClientHandle) {
    SPDLOG_LOGGER_ERROR(g_logger, "Error opening JACK status={}.", status);
    throw ServerNotRunningException();
  }

  // Register a function to be called if and when the JACK server shuts down the client thread.
  jack_on_shutdown(g_jackClientHandle, jackShutdownCallback, nullptr);
  g_stateFlag = State::idle;
}
/**
 * Tell the Jack server to stop calling the processCallback function.
 * This client will be removed from the process graph. All ports belonging to
 * this client are closed.
 *
 * After this function returns, the `jackClient` is back into the `idle` state.
 */
void stop() noexcept {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  SPDLOG_LOGGER_TRACE(g_logger, "jackClient::stop");
  stopInternal();
}

/**
 * Indicates the current state of the `jackClient`.
 *
 * This function will block as long as a state-change is ongoing.
 *
 * @return the current state of the `jackClient`.
 */
State state() {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  return stateInternal();
}

/**
 * Start a new session.
 *
 * The processCallback function will be invoked by JACK server on each cycle.
 *
 * This function can only be called from the `connected` state.
 * After this function succeeds, the `jackClient` is in `running` state.
 *
 * @throws BadStateException - if this function is called on a state other than `idle`.
 * @throws ServerException - if the JACK server has encountered a problem.
 */
void activate() noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  SPDLOG_LOGGER_TRACE(g_logger, "jackClient::activate");
  if (g_stateFlag != State::idle) {
    throw BadStateException("Cannot activate JACK client. Wrong state " +
                            stateAsString(g_stateFlag));
  }

  int err = jack_activate(g_jackClientHandle);
  if (err) {
    throw ServerException("Failed to activate JACK client!");
  }

  g_stateFlag = State::running;
}
/**
 * Register a handler that shall be called when the server is ending abnormally.
 * @param handler - the function to be called
 * @throws BadStateException - if this function is called from a state other than `idle`.
 */
void onServerAbend(const OnServerAbendHandler &handler) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  SPDLOG_LOGGER_TRACE(g_logger, "jackClient::registerProcessCallback");
  if (g_stateFlag != State::idle) {
    throw BadStateException("Cannot register callback. Wrong state " + stateAsString(g_stateFlag));
  }
  g_onServerAbendHandler = handler;
}
/**
 * Create a new Clock that gets its timing from the JACK server.
 * @return a smart pointer holding the clock.
 * @throws BadStateException - if the `jackClient` is in `closed` state.
 */
a2jmidi::ClockPtr clock() {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  SPDLOG_LOGGER_TRACE(g_logger, "jackClient::getClock");
  if (g_stateFlag == State::closed) {
    throw BadStateException("Cannot get Clock. Wrong state " + stateAsString(g_stateFlag));
  }
  return std::make_unique<JackClock>();
}
/**
 * Tell the Jack server to call the given processCallback function on each cycle.
 *
 * `registerProcessCallback()` can only be called from the `idle` state.
 *
 * @param processCallback - the function to be called
 * @throws BadStateException - if this function is called from a state other than `connected`.
 * @throws ServerException - if the JACK server has encountered an other problem.
 */
void registerProcessCallback(const ProcessCallback &processCallback) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  SPDLOG_LOGGER_TRACE(g_logger, "jackClient::registerProcessCallback");
  if (g_stateFlag != State::idle) {
    throw BadStateException("Cannot register callback. Wrong state " + stateAsString(g_stateFlag));
  }
  g_customCallback = processCallback;
  int err = jack_set_process_callback(g_jackClientHandle, jackInternalCallback, nullptr);
  if (err) {
    throw ServerException("JACK error when registering callback.");
  }
}
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
JackPort newSenderPort(const std::string &portName) noexcept(false) {
  std::unique_lock<std::mutex> lock{g_stateAccessMutex};
  if (g_stateFlag != State::idle) {
    throw BadStateException("Cannot create new SenderPort. Wrong state " +
                            stateAsString(g_stateFlag));
  }
  auto *result = jack_port_register(g_jackClientHandle, portName.c_str(), JACK_DEFAULT_MIDI_TYPE,
                                    JackPortIsOutput, 0);
  if (!result) {
    throw std::runtime_error("Failed to create JACK MIDI port!\n");
  }
  SPDLOG_LOGGER_TRACE(g_logger, "jackClient::newSenderPort - port \"{}\" created.", portName);
  return result;
}
} // namespace jackClient