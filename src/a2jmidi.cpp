/*
 * File: a2jmidi.cpp
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
#include "a2jmidi.h"
#include "alsa_client.h"
#include "jack_client.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <cmath>
#include <iostream>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <signal.h>
#include <thread>

namespace a2jmidi {

static auto g_logger = spdlog::stdout_color_mt("a2jmidi");


static bool g_continue{true};



#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-lambda-function-name"
void open(const std::string &clientNameProposal, const std::string &connectTo,
          bool startJack) noexcept(false) {
  SPDLOG_LOGGER_TRACE(g_logger,"a2jmidi::open");

  jackClient::open(clientNameProposal, startJack);
  const std::string clientName = jackClient::clientName();
  auto *jackPort = jackClient::newSenderPort(clientName);

  alsaClient::open(clientName);
  alsaClient::newReceiverPort(clientName, connectTo);

  auto forEachJackPeriod = [jackPort](int nFrames, sysClock::TimePoint deadLine) -> int {
    void *pPortBuffer = jack_port_get_buffer(jackPort, nFrames);
    jack_midi_clear_buffer(pPortBuffer);

    auto forEachMidiDo = [pPortBuffer, deadLine, nFrames](const midi::Event &event,
                                                        sysClock::TimePoint timeStamp) -> int {
      auto leadTime = deadLine - timeStamp; // how much is the event ahead of the deadline
      int leadFrames = ceil(jackClient::impl::duration2frames(leadTime));
      int eventPos = nFrames - leadFrames; // the position in the frame buffer
      if (eventPos < 0) {
        SPDLOG_LOGGER_ERROR(g_logger,"a2j_midi - buffer underrun by {} frames.", -eventPos);
        eventPos = 0;
      }
      if (eventPos >= nFrames) {
        SPDLOG_LOGGER_ERROR(g_logger,"a2j_midi - buffer overrun by {} frames.", eventPos - nFrames);
        eventPos = nFrames - 1;
      }

      int evLength = event.size();
      const auto *pMidiData = &event[0];

      int err = jack_midi_event_write(pPortBuffer, eventPos, pMidiData, evLength);
      if (err == -ENOBUFS) {
        SPDLOG_LOGGER_ERROR(g_logger,"a2j_midi - JACK write error ({} bytes did not fit in buffer).", evLength);
        return -1; //
      }
      if (err == -EINVAL) {
        SPDLOG_LOGGER_ERROR(g_logger,"a2j_midi - JACK write error (invalid argument).");
        return -1; //
      }
      if (err != 0) {
        SPDLOG_LOGGER_ERROR(g_logger,"a2j_midi - JACK write error (undocumented error-code {}).", err);
        return -1; //
      }
      SPDLOG_LOGGER_TRACE(g_logger,"a2j_midi::forEachMidiDo - event[{}] written to buffer.",evLength);
      return 0;
    };
    return alsaClient::retrieve(deadLine, forEachMidiDo);
  };


  jackClient::registerProcessCallback(forEachJackPeriod);
  jackClient::onServerAbend([]() { g_continue=false; });

  alsaClient::activate();
  jackClient::activate();
}
#pragma clang diagnostic pop

void close() {
  SPDLOG_LOGGER_TRACE(g_logger,"a2jmidi::close");
  jackClient::close();
  alsaClient::close();
}
void configureLogging(){
  // set log pattern
  spdlog::set_pattern("[PID %P] [%T.%e] [%s:%#] %l: %v");
  // Set global log level
  spdlog::set_level(spdlog::level::info);
  // Set a different log levels for selected files
  spdlog::get("a2jmidi")->set_level(spdlog::level::debug);
  spdlog::get("jack_client")->set_level(spdlog::level::debug);

}
void sigtermHandler(int sig) {
  if (sig == SIGTERM) {
    g_continue = false;
    SPDLOG_LOGGER_TRACE(g_logger,"a2jmidi::sigintHandler - SIGTERM received");
  }
  signal(SIGTERM, sigtermHandler); // reinstall handler
}
void sigintHandler(int sig) {
  if (sig == SIGINT) {
    g_continue = false;
    SPDLOG_LOGGER_TRACE(g_logger,"a2jmidi::sigintHandler - SIGINT received");
  }
  signal(SIGINT, sigintHandler); // reinstall handler
}
int run(const std::string &clientNameProposal, const std::string &connectTo,
        bool startJack) noexcept {
  using namespace std::chrono_literals;
  try {
    SPDLOG_LOGGER_TRACE(g_logger,"a2jmidi::run");
    open(clientNameProposal, connectTo, startJack);

    // install signal handlers for shutdown.
    signal(SIGINT, sigintHandler); // Ctrl-C interrupt the application. Usually causing it to abort.
    signal(SIGTERM, sigtermHandler); // cleanup and terminate the process
    //pause(); // suspend this thread until a signal (e.g. SIGINT via Ctrl-C) is received
    while(g_continue) {
      std::this_thread::sleep_for(100ms);
    }

    close();

    return 0;
  } catch (const std::runtime_error &re) {
    std::cerr << "Runtime error: " << re.what() << std::endl;
  } catch (const std::exception &ex) {
    std::cerr << "Error occurred: " << ex.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown failure occurred." << std::endl;
  }
  return 1;
}

int run(const CommandLineInterpretation &arguments) noexcept {

  configureLogging();

  switch (arguments.action) {
  case CommandLineAction::messageError:
    std::cout << arguments.message.str();
    return 1;
  case CommandLineAction::messageOK:
    std::cout << arguments.message.str();
    return 0;
  case CommandLineAction::run:
    return run(arguments.clientName, arguments.connectTo, arguments.startJack);
  }
}

} // namespace a2jmidi