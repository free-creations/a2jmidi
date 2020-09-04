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
#include <jack/jack.h>
#include <jack/midiport.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>

// this should be large enough to hold the largest MIDI message to be encoded by the
// AlsaMidiEventParser
#define MAX_MIDI_EVENT_SIZE 16

// A number of frames used to compensate for jitter between the ALSA timing and the JACK timing
// set around two millisecond at 320000 samples/sec
#define SECURITY_FRAMES 64

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
 * Print an error message to stderr, and die.
 * @param format a format string
 * @param ... the arguments
 */
void fatal(const char *format, ...) {
  va_list ap;

  fprintf(stderr, "[%s] ", clientName);

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}

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
    fatal("Cannot %s - %s", operation, snd_strerror(alsaResult));
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
const char *openJackServer(const char *clientNameHint) {

  jack_status_t status;

  hJackClient = jack_client_open(clientNameHint, JackNoStartServer, &status);
  if (hJackClient == nullptr) {
    SPDLOG_ERROR("*** Please start the JACK server, prior to start this program. ***");
    fatal("*** Please start the JACK server, prior to start this program. ***\n");
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
    fatal("Failed to create JACK MIDI port!\n");
  }
  SPDLOG_TRACE("a2j_remake::newOutputJackPort - port \"{}\" created.", portName);
}

int processAlsaEvent(const snd_seq_event_t &alsaEvent, jack_nframes_t cycleLength,
                     void *pPortBuffer, jack_nframes_t framesBeforeCycleStart) {

  jack_nframes_t eventPos = cycleLength - framesBeforeCycleStart;
  if (eventPos < 0) {
    fprintf(stderr, "SERIOUS: invalid parameter.\n");
    return -1;
  }
  if (alsaEvent.type == SND_SEQ_EVENT_PORT_SUBSCRIBED) {
    // A new port has subscribed to us.
    // Now, ALSA will deliver all events that this port has send so far, but we have not received
    // yet. These events come in *one big burst* and will blow up the membranes of our speakers. So
    // drop them all!!!
    snd_seq_drop_input_buffer(hSequencer);
    snd_midi_event_reset_decode(hAlsaMidiEventParser);
    return 0;
  }

  // decode the ALSA event.
  unsigned char pMidiData[MAX_MIDI_EVENT_SIZE];
  long evLength =
      snd_midi_event_decode(hAlsaMidiEventParser, pMidiData, MAX_MIDI_EVENT_SIZE, &alsaEvent);
  if (evLength <= 0) {
    return 0; // the alsa event does not correspond to one or more MIDI messages.
  }

  SPDLOG_TRACE("a2j_remake::processAlsaEvent - midi event received");
  int err = jack_midi_event_write(pPortBuffer, eventPos, pMidiData, evLength);
  if (err == ENOBUFS) {
    fprintf(stderr, "SERIOUS[%s]: JACK write error (%ld bytes did not fit in buffer).\n",
            clientName, evLength);
    return 0; // we still continue...
  }
  if (err != 0) {
    fprintf(stderr, "SERIOUS[%s]: JACK write error (undocumented error-code %d).\n", clientName,
            err);
    return 0; // we still continue...
  }
  return 0;
}

using Sys_Microseconds = std::chrono::microseconds;

alsaReceiverQueue::TimePoint currentCycleStart() {
  jack_nframes_t offsetFrames = jack_frames_since_cycle_start(hJackClient);
  jack_nframes_t  sampleRate = jack_get_sample_rate(hJackClient);
  auto jackOffset = Sys_Microseconds(offsetFrames * 1000000 / sampleRate);
  return alsaReceiverQueue::Sys_clock::now() - jackOffset;
}

jack_nframes_t duration2frames(Sys_Microseconds duration) {
  jack_nframes_t  sampleRate = jack_get_sample_rate(hJackClient);
  return (sampleRate * duration.count() ) / 1000000;
}

int jackReceiverCallback(jack_nframes_t cycleLength, void *arg) {

  void *portBuffer = jack_port_get_buffer(hJackPort, cycleLength);
  jack_midi_clear_buffer(portBuffer);
  int err = 0;
  auto cycleStart = currentCycleStart();

  alsaReceiverQueue::process(
      cycleStart, //
      ([&](const snd_seq_event_t &event, alsaReceiverQueue::TimePoint timeStamp) {
        Sys_Microseconds duration = std::chrono::duration_cast<Sys_Microseconds>(cycleStart - timeStamp);
        err = processAlsaEvent(event, cycleLength,portBuffer, duration2frames(duration));
      }));

  return err;
}

void jackErrorCallback(const char *msg){
  SPDLOG_INFO("a2j_remake::jackErrorCallback - {}",msg);
}

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
  spdlog::set_level(spdlog::level::trace);
  const char *clientNameHint;

  SPDLOG_TRACE("a2j_remake::main");
  if (argc == 2) {
    clientNameHint = argv[1];
  } else {
    clientNameHint = "a2j_remake";
  }

  int err;

  jack_set_error_function (jackErrorCallback);
  // initialize
  clientName = openJackServer(clientNameHint);

  newOutputJackPort("playback");

  newAlsaMidiEventParser();

  openAlsaSequencer(clientName);

  newInputAlsaPort("capture");

  snd_seq_drain_output(hSequencer);
  alsaReceiverQueue::start(hSequencer);

  jack_set_process_callback(hJackClient, jackReceiverCallback, nullptr);

  err = jack_activate(hJackClient);
  if (err) {
    fatal("Failed to activate JACK client!\n");
  }

  // install signal handlers for shutdown.
  signal(SIGINT, sigintHandler);
  signal(SIGTERM, sigtermHandler);
  pause(); // suspend this thread until a signal (e.g. SIGINT via Ctrl-C) is received

  alsaReceiverQueue::stop();
  jack_deactivate(hJackClient);
  closeJackServer();
  SPDLOG_TRACE("a2j_remake::main - end.");
  return 0;
}
