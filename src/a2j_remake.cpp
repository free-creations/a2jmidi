/*
 * File: a2j_remake.cpp
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

#include "alsa_receiver_queue.h"
#include "spdlog/spdlog.h"

#include <alsa/asoundlib.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

// this should be large enough to hold the largest MIDI message to be encoded by the
// AlsaMidiEventParser
#define MAX_MIDI_EVENT_SIZE 16

// The following values are needed inside the JACK callback.
// For the sake of simplicity we define them as global static values.
// Please bear in mind:
//
// - these values must all be set *before* `jack_activate` is called
// - once `jack_activate` is called these values are immutable.
//
static int queueId;                            /// the ID-number of the ALSA input queue
static int portId;                             /// the ID-number of our ALSA input port
static jack_client_t *hJackClient = nullptr;   /// handle to access the JACK server
static jack_port_t *hJackPort;                 /// handle to access our JACK output port
static snd_seq_t *hSequencer;                  /// handle to access the ALSA sequencer
static snd_midi_event_t *hAlsaMidiEventParser; /// handle to access the ALSA MIDI parser
static const char *clientName;                 /// name of this client

static const char *version = "0.0.1";

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

/**
 * Open the ALSA sequencer in non-blocking mode.
 * @param clientName The name of the new client or NULL.
 * @Note implicit-return - the global variable `hSequencer`is set.
 */
void openAlsaSequencer(const char *alsaClientName) {
  int err;
  // open sequencer (we need a duplex stream, in order to start and stop the queue).
  err = snd_seq_open(&hSequencer, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  checkAlsa("open sequencer", err);

  // set our client's name
  err = snd_seq_set_client_name(hSequencer, alsaClientName);
  checkAlsa("set client name", err);
  SPDLOG_TRACE("a2j_remake::openAlsaSequencer - client \"{}\" opened.", alsaClientName);
}

/**
 * Open the JACK server.
 * @param clientNameHint - a name for the new client. If this name is not unique,
 * JACK will generate a unique client name.
 * @Note implicit-return - the global variable `hJackClient`is set.
 * @return the actual name of this client
 */
const char *openJackServer(const char *clientNameHint) noexcept(false) {
  jack_status_t status;

  hJackClient = jack_client_open(clientNameHint, JackNoStartServer, &status);
  if (hJackClient == nullptr) {
    throw std::runtime_error("[" __FILE__ ":" + std::to_string(__LINE__) +
                             "] Cannot open the client. JACK server is not running.");
  }

  // in case JACK had a better (unique) name.
  const char *actualClientName = jack_get_client_name(hJackClient);
  SPDLOG_TRACE("a2j_remake::openJackServer - client \"{}\" opened.", actualClientName);
  return actualClientName;
}

void closeJackServer() {
  if (hJackClient) {
    SPDLOG_TRACE("a2j_remake::closeJackServer - closing \"{}\".", clientName);
    int err = jack_deactivate(hJackClient);
    if (err) {
      SPDLOG_ERROR("*** Please start the JACK server, prior to start this program. ***");
    }
  }
  hJackClient = nullptr;
}

/**
 * Create a new ALSA port. External applications can write to this port.
 * @note implicit-param hSequencer handle to the ALSA midi sequencer.
 * @param portName name of the new port.
 * @note implicit-param queueId - the queue from which the port shall get the time stamps.
 * @note implicit-return - the new port number.
 */
void newInputAlsaPort(const char *portName) {
  portId = snd_seq_create_simple_port(hSequencer, portName,
                                      SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                      SND_SEQ_PORT_TYPE_APPLICATION);
  checkAlsa("createInputPort", portId);
  SPDLOG_TRACE("a2j_remake::newInputAlsaPort - port \"{}\" created.", portName);
}

/**
 * Install a MIDI parser object that can be used to convert ALSA-sequencer events to MIDI bytes.
 * @note implicit-return - the variable `hAlsaMidiEventParser` receives a handle to the parser.
 */
void newAlsaMidiEventParser() {

  int err = snd_midi_event_new(MAX_MIDI_EVENT_SIZE, &hAlsaMidiEventParser);
  checkAlsa("create ALSA-MIDI parser", err);

  snd_midi_event_reset_decode(hAlsaMidiEventParser);
  snd_midi_event_no_status(hAlsaMidiEventParser, 1); // no running status byte!!!
  SPDLOG_TRACE("a2j_remake::newAlsaMidiEventParser -  created.");
}

/**
 * Create a new JACK MIDI port. External applications can read from this port.
 * @note implicit-param hJackClient - handle to the JACK audio server.
 * @param portName portName name of the new port.
 * @note implicit-return - the variable `hJackPort` receives a handle to the new port.
 */
void newOutputJackPort(const char *portName) {
  hJackPort =
      jack_port_register(hJackClient, portName, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  if (!hJackPort) {
    throw std::runtime_error("Failed to create JACK MIDI port!\n");
  }
  SPDLOG_TRACE("a2j_remake::newOutputJackPort - port \"{}\" created.", portName);
}

int processAlsaEvent(const snd_seq_event_t &alsaEvent, jack_nframes_t cycleLength,
                     void *pPortBuffer, double framesBeforeCycleStart) {

  jack_nframes_t eventPos = floor(cycleLength - framesBeforeCycleStart);
  if (eventPos < 0) {
    SPDLOG_WARN("a2j_remake::processAlsaEvent - event too late by {} frames.", -eventPos);
    eventPos = 0;
  }
  if (eventPos >= cycleLength) {
    SPDLOG_WARN("a2j_remake::processAlsaEvent - event pos {} exceeds buffer of {}.", eventPos,
                cycleLength);
    eventPos = cycleLength - 1;
  }
  if (alsaEvent.type == SND_SEQ_EVENT_PORT_SUBSCRIBED) {
    // A new port has subscribed to us.
    // Now, ALSA will deliver all events that this port has send so far, but we have not received
    // yet. These events come in *one big burst* and will blow up the membranes of our speakers. So
    // drop them all!!!
    snd_seq_drop_input_buffer(hSequencer);
    snd_midi_event_reset_decode(hAlsaMidiEventParser);
    SPDLOG_INFO("a2j_remake::processAlsaEvent - A port has subscribed, events dropped. ");
    return 0;
  }

  // decode the ALSA event.
  unsigned char pMidiData[MAX_MIDI_EVENT_SIZE];
  long evLength =
      snd_midi_event_decode(hAlsaMidiEventParser, pMidiData, MAX_MIDI_EVENT_SIZE, &alsaEvent);
  if (evLength <= 0) {
    if (evLength == -ENOENT) {
      // The sequencer event does not correspond to one or more MIDI messages.
      return 0; // that's OK ... just ignore
    }
    SPDLOG_ERROR("a2j_remake::processAlsaEvent - snd_midi_event_decode - error {}.", evLength);
    return -1;
  }

  SPDLOG_TRACE("a2j_remake::processAlsaEvent - midi event received");
  int err = jack_midi_event_write(pPortBuffer, eventPos, pMidiData, evLength);
  if (err == -ENOBUFS) {
    SPDLOG_ERROR(
        "a2j_remake::processAlsaEvent - JACK write error ({} bytes did not fit in buffer).",
        evLength);
    return -1; //
  }
  if (err == -EINVAL) {
    SPDLOG_ERROR("a2j_remake::processAlsaEvent - JACK write error (invalid argument).");
    return -1; //
  }
  if (err != 0) {
    SPDLOG_ERROR("a2j_remake::processAlsaEvent - JACK write error (undocumented error-code {}).",
                 err);
    return -1; //
  }
  return 0;
}

using Sys_Microseconds = std::chrono::microseconds;

alsaReceiverQueue::TimePoint currentCycleStart() {
  jack_nframes_t offsetFrames = jack_frames_since_cycle_start(hJackClient);
  jack_nframes_t sampleRate = jack_get_sample_rate(hJackClient);
  auto jackOffset = Sys_Microseconds(offsetFrames * 1000000 / sampleRate);
  return alsaReceiverQueue::Sys_clock::now() - jackOffset;
}

double duration2frames(const Sys_Microseconds duration) {
  jack_nframes_t sampleRate = jack_get_sample_rate(hJackClient);
  return ((double)(sampleRate * duration.count())) / 1000000.0;
}

static alsaReceiverQueue::TimePoint previousCycleStart;
static constexpr auto SYS_JITTER = Sys_Microseconds(250);

int jackReceiverCallback(jack_nframes_t cycleLength, void *arg) {

  const auto sysFrameJitter = duration2frames(SYS_JITTER);

  void *portBuffer = jack_port_get_buffer(hJackPort, cycleLength);
  jack_midi_clear_buffer(portBuffer);
  int err = 0;

  auto cycleStart = currentCycleStart();

  // for debugging purposes, we'll check, that we always advance by cycleLength frames.
  double framesSincePrevious = duration2frames(
      std::chrono::duration_cast<Sys_Microseconds>(cycleStart - previousCycleStart));
  previousCycleStart = cycleStart;
  double actualFrameJitter = cycleLength - framesSincePrevious;
  if (std::abs(actualFrameJitter) > sysFrameJitter) {
    spdlog::error("a2j_remake::jackReceiverCallback - huge frame-jitter of {} frames.",
                  actualFrameJitter, sysFrameJitter);
    return 0;
  }

  // all events up to (but not including) this cycle-start shall be processed.
  // It is important not to steel events from the next cycle.
  // Therefore we'll not process events that were registered after a sysJitter time range
  // around currentCycleStart.
  auto deadline = currentCycleStart() - SYS_JITTER;

  alsaReceiverQueue::process(
      deadline, //
      ([&](const snd_seq_event_t &event, alsaReceiverQueue::TimePoint timeStamp) {
        Sys_Microseconds beforeCycleStart =
            std::chrono::duration_cast<Sys_Microseconds>(cycleStart - timeStamp);
        if (beforeCycleStart.count() > 0) {
          err = processAlsaEvent(event, cycleLength, portBuffer, duration2frames(beforeCycleStart));
        } else {
          spdlog::error("a2j_remake::jackReceiverCallback - event is {} us after cycle-start.",
                        -beforeCycleStart.count());
        }
      }));

  return err;
}

void jackErrorCallback(const char *msg) { SPDLOG_INFO("a2j_remake::jackErrorCallback - {}", msg); }

void sigtermHandler(int sig) {
  if (sig == SIGTERM) {
    SPDLOG_TRACE("a2j_remake::sigintHandler - SIGTERM received");
  }
  signal(SIGTERM, sigtermHandler); // reinstall handler
}
void sigintHandler(int sig) {
  if (sig == SIGINT) {
    SPDLOG_TRACE("a2j_remake::sigintHandler - SIGINT received");
  }
  signal(SIGINT, sigintHandler); // reinstall handler
}

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::info);
  const char *clientNameHint;

  if (argc == 2) {
    clientNameHint = argv[1];
  } else {
    clientNameHint = "a2j_remake";
  }

  int err;

  jack_set_error_function(jackErrorCallback);
  // initialize
  clientName = openJackServer(clientNameHint);

  newOutputJackPort("playback");

  newAlsaMidiEventParser();

  openAlsaSequencer(clientName);

  newInputAlsaPort("capture");

  jack_set_process_callback(hJackClient, jackReceiverCallback, nullptr);

  err = jack_activate(hJackClient);
  if (err) {
    throw std::runtime_error("Failed to activate JACK client!");
  }

  snd_seq_drain_output(hSequencer);
  alsaReceiverQueue::start(hSequencer);

  // install signal handlers for shutdown.
  signal(SIGINT, sigintHandler);
  signal(SIGTERM, sigtermHandler);
  pause(); // suspend this thread until a signal (e.g. SIGINT via Ctrl-C) is received

  alsaReceiverQueue::stop();

  closeJackServer();
  SPDLOG_TRACE("a2j_remake::main - end.");
  return 0;
}
