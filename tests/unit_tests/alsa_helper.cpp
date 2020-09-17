/*
 * File: alsa_helper.cpp
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
#include "alsa_helper.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <stdexcept>
#include <thread>

namespace unitTestHelpers {

/**
 * Error handling for ALSA functions.
 * ALSA function often return the error code as a negative result. This function
 * checks the result for negativity.
 * - if negative, it prints the error message and dies.
 * - if positive or zero it does nothing.
 * @param operation description of the operation that was attempted.
 * @param alsaResult possible error code from an ALSA call.
 */
void AlsaHelper::checkAlsa(const char *operation, int alsaResult) {
  if (alsaResult < 0) {
    SPDLOG_CRITICAL("Cannot {} - {}", operation, snd_strerror(alsaResult));
    throw std::runtime_error("ALSA problem");
  }
}

snd_seq_t *AlsaHelper::g_hSequencer{nullptr}; /// handle to access the ALSA sequencer
int AlsaHelper::g_clientId{0};                /// the client-number of this client

/**
 * Open the ALSA sequencer in non-blocking mode.
 */
void AlsaHelper::openAlsaSequencer() {
  int err;
  // open sequencer (we need a duplex stream, in order to startNextFuture and stop the queue).
  err = snd_seq_open(&g_hSequencer, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  checkAlsa("open sequencer", err);

  // set our client's name
  err = snd_seq_set_client_name(g_hSequencer, "a_j_midi-tests");
  checkAlsa("snd_seq_set_client_name", err);

  g_clientId = snd_seq_client_id(g_hSequencer);
  SPDLOG_TRACE("AlsaHelper::openAlsaSequencer - client {} created.", g_clientId);
}

snd_seq_t* AlsaHelper::getSequencerHandle(){
    return g_hSequencer;
}

/**
 * The `carryOnFlag` is true as long as the listening-thread shall run.
 * Setting the `carryOnFlag` to false will stop the listening-thread.
 */
std::atomic<bool> g_carryOnFlag{false};
/**
 * Close the ALSA sequencer.
 * This can only be called when event listening has stopped.
 */
void AlsaHelper::closeAlsaSequencer() {
  // verify that the listening queue is stopped for sure.
  if (g_carryOnFlag) {
    SPDLOG_CRITICAL(
        "AlsaHelper::closeAlsaSequencer - attempt to stop while listening threads still run.");
    throw std::runtime_error("Sequencer cannot be stopped while listening threads still run.");
  }

  // kind of dirty hack!!!
  // Even when "carryOnFlag==false" there is a small chance that the
  // "listenForEventsLoop" accesses the sequencer for one
  // last time. To avoid this, lets sleep for a while.
  std::this_thread::sleep_for(std::chrono::milliseconds(SHUTDOWN_POLL_PERIOD_MS));

  SPDLOG_TRACE("AlsaHelper::closeAlsaSequencer - closing client {}.", g_clientId);
  int err = snd_seq_close(g_hSequencer);
  checkAlsa("snd_seq_close", err);
}

/**
 * create an output (emitting) port.
 * @param portName the name of the port
 * @return a handle for the port.
 */
int AlsaHelper::createOutputPort(const char *portName) {
  int portId; /// the ID-number of our ALSA input port
  portId = snd_seq_create_simple_port(g_hSequencer, portName,
                                      SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                      SND_SEQ_PORT_TYPE_APPLICATION);
  SPDLOG_TRACE("AlsaHelper::createOutputPort - port {} created.", portId);
  return portId;
}

/**
 * create an input (receiving) port.
 * @param portName the name of the port
 * @return a handle (port number) for the new port.
 */
int AlsaHelper::createInputPort(const char *portName) {
  int portId; /// the ID-number of our ALSA input port
  portId = snd_seq_create_simple_port(g_hSequencer, portName,
                                      SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                      SND_SEQ_PORT_TYPE_APPLICATION);
  checkAlsa("createInputPort", portId);
  SPDLOG_TRACE("AlsaHelper::createInputPort - port {} created.", portId);
  return portId;
}

/**
 * Connect an output-port to an input-port.
 * Both ports must belong to this client.
 * @param hEmitterPort the port-number of the output-port.
 * @param hReceiverPort the port-number of the input-port.
 */
void AlsaHelper::connectPorts(int hEmitterPort, int hReceiverPort) {
  snd_seq_addr_t emitter, receiver;
  snd_seq_port_subscribe_t *pSubscriptionDescription;

  emitter.client = g_clientId;
  emitter.port = hEmitterPort;

  receiver.client = g_clientId;
  receiver.port = hReceiverPort;

  snd_seq_port_subscribe_alloca(&pSubscriptionDescription);
  snd_seq_port_subscribe_set_sender(pSubscriptionDescription, &emitter);
  snd_seq_port_subscribe_set_dest(pSubscriptionDescription, &receiver);
  auto err = snd_seq_subscribe_port(g_hSequencer, pSubscriptionDescription);
  checkAlsa("connectPorts", err);
  SPDLOG_TRACE("AlsaHelper::connectPorts - EmitterPort {} connected to ReceiverPort {}.",
               hEmitterPort, hReceiverPort);
}

/**
 * Connect an internal input-port to an external port.
 * Use `$ aconnect -l` to list available ports.
 * @param externalClientId the client id of the external device.
 * @param hExternalPort the port-number of the external port.
 * @param hReceiverPort the port-number of the internal input-port.
 */
void AlsaHelper::connectExternalPort(int externalClientId, int hExternalPort, int hReceiverPort) {
  snd_seq_addr_t emitter, receiver;
  snd_seq_port_subscribe_t *pSubscriptionDescription;

  emitter.client = externalClientId;
  emitter.port = hExternalPort;

  receiver.client = g_clientId;
  receiver.port = hReceiverPort;

  snd_seq_port_subscribe_alloca(&pSubscriptionDescription);
  snd_seq_port_subscribe_set_sender(pSubscriptionDescription, &emitter);
  snd_seq_port_subscribe_set_dest(pSubscriptionDescription, &receiver);
  auto err = snd_seq_subscribe_port(g_hSequencer, pSubscriptionDescription);
  checkAlsa("connectPorts", err);

  SPDLOG_TRACE("AlsaHelper::connectExternalPort - ExternalPort {}:{} connected to ReceiverPort {}.",
               externalClientId, hExternalPort, hReceiverPort);
}
/**
 * Sends Midi events through the given emitter port.
 *
 * The events are note-on and note-off on channel zero.
 *
 * This call is blocking. That means, control will be given back
 * to the caller once all events have been send.
 * @param hEmitterPort the port-number of the emitter port.
 * @param eventCount the number of events to send.
 * @param intervalMs the time (in milliseconds) to between two "note on" events.
 */
void AlsaHelper::sendEvents(int hEmitterPort, int eventCount, long intervalMs) {
  SPDLOG_TRACE("AlsaHelper::sendEvents");

  long noteOnTime = intervalMs / 2;
  long noteOffTime = intervalMs - noteOnTime;

  snd_seq_event_t evNoteOn1;
  snd_seq_ev_set_subs(&evNoteOn1);
  snd_seq_ev_set_direct(&evNoteOn1);
  snd_seq_ev_set_source(&evNoteOn1, hEmitterPort);
  snd_seq_ev_set_noteon(&evNoteOn1, 0, 60, 64);
  
  snd_seq_event_t evNoteOn2;
  snd_seq_ev_set_subs(&evNoteOn2);
  snd_seq_ev_set_direct(&evNoteOn2);
  snd_seq_ev_set_source(&evNoteOn2, hEmitterPort);
  snd_seq_ev_set_noteon(&evNoteOn2, 0, 67, 64);

  snd_seq_event_t evNoteOff1;
  snd_seq_ev_set_subs(&evNoteOff1);
  snd_seq_ev_set_direct(&evNoteOff1);
  snd_seq_ev_set_source(&evNoteOff1, hEmitterPort);
  snd_seq_ev_set_noteoff(&evNoteOff1, 0, 60, 0);

  snd_seq_event_t evNoteOff2;
  snd_seq_ev_set_subs(&evNoteOff2);
  snd_seq_ev_set_direct(&evNoteOff2);
  snd_seq_ev_set_source(&evNoteOff2, hEmitterPort);
  snd_seq_ev_set_noteoff(&evNoteOff2, 0, 67, 0);

  for (int i = 0; i < eventCount; ++i) {
    auto err = snd_seq_event_output_direct(g_hSequencer, &evNoteOn1);
    checkAlsa("snd_seq_event_output_direct", err);
     err = snd_seq_event_output_direct(g_hSequencer, &evNoteOn2);
    checkAlsa("snd_seq_event_output_direct", err);

    err = snd_seq_drain_output(g_hSequencer);
    checkAlsa("snd_seq_drain_output", err);
    SPDLOG_TRACE("AlsaHelper::sendEvents - 2 Note-Ons sent.");

    std::this_thread::sleep_for(std::chrono::milliseconds(noteOnTime));

    err = snd_seq_event_output_direct(g_hSequencer, &evNoteOff2);
    checkAlsa("snd_seq_event_output_direct", err);
    err = snd_seq_event_output_direct(g_hSequencer, &evNoteOff1);
    checkAlsa("snd_seq_event_output_direct", err);

    err = snd_seq_drain_output(g_hSequencer);
    checkAlsa("snd_seq_drain_output", err);

    std::this_thread::sleep_for(std::chrono::milliseconds(noteOffTime));
  }
}

int AlsaHelper::retrieveEvents() {
  SPDLOG_TRACE("AlsaHelper::retrieveEvents");
  snd_seq_event_t *ev;
  int eventCount = 0;
  int status;

  do {
    status = snd_seq_event_input(g_hSequencer, &ev);
    switch (status) {
    case -EAGAIN:        // FIFO empty, try again later
      return eventCount; // FIFO of sequencer overran, and some events are lost.
    default:             //
      checkAlsa("snd_seq_event_input", status);
    }

    if (ev) {
      switch (ev->type) {
      case SND_SEQ_EVENT_NOTEON: //
        eventCount++;
        SPDLOG_TRACE("AlsaHelper::retrieveEvents - got Event(Note on)");
        break;
      default: //
        SPDLOG_TRACE("AlsaHelper::retrieveEvents -  got Event(other)");
      }
    }
  } while (status > 0);

  return eventCount;
}

int AlsaHelper::listenForEventsLoop(snd_seq_t *pSndSeq) {
  SPDLOG_TRACE("AlsaHelper::listenForEventsLoop");
  int eventCount = 0;

  // lets create the poll descriptors that we will need when we wait for incoming events.
  int fdsCount = snd_seq_poll_descriptors_count(pSndSeq, POLLIN);
  struct pollfd fds[fdsCount];

  while (g_carryOnFlag) {
    auto err = snd_seq_poll_descriptors(pSndSeq, fds, fdsCount, POLLIN);
    checkAlsa("snd_seq_poll_descriptors", err);
    auto hasEvents = poll(fds, fdsCount, SHUTDOWN_POLL_PERIOD_MS);
    if (hasEvents > 0) {
      SPDLOG_TRACE("AlsaHelper::listenForEventsLoop - poll signaled {} event.", hasEvents);
      eventCount = eventCount + retrieveEvents();
    } else {
      SPDLOG_TRACE("AlsaHelper::listenForEventsLoop - poll timed out.");
    }
  }
  return eventCount;
}

FutureEventCount AlsaHelper::startEventReceiver() {
  SPDLOG_TRACE("AlsaHelper::startEventReceiver");
  g_carryOnFlag = true;
  return std::async(std::launch::async,
                    [pSndSeq = g_hSequencer]() -> int { return listenForEventsLoop(pSndSeq); });
}

void AlsaHelper::stopEventReceiver(FutureEventCount &future) {
  SPDLOG_TRACE("AlsaHelper::stopEventReceiver");
  // stop the listenForEventsLoop
  g_carryOnFlag = false;
  // wait until the FutureEventCount has become ready.
  auto status = future.wait_for(std::chrono::milliseconds(2 * SHUTDOWN_POLL_PERIOD_MS));
  if (status == std::future_status::timeout) {
    throw std::runtime_error("Receiver cannot be stopped- does not timeout.");
  }
}

} // namespace unitTestHelpers