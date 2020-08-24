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

namespace unit_test_helpers {

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
    spdlog::critical("Cannot {} - {}", operation, snd_strerror(alsaResult));
    throw std::runtime_error("Something Bad happened here");
  }
}

snd_seq_t *AlsaHelper::hSequencer{nullptr}; /// handle to access the ALSA sequencer
int AlsaHelper::clientId{0};                /// the client-number of this client
// struct pollfd *AlsaHelper::pPollDescriptor{nullptr};
// int AlsaHelper::pollDescriptorsCount{0};

/**
 * Open the ALSA sequencer in ???? mode.
 */
void AlsaHelper::openAlsaSequencer() {
  int err;
  // open sequencer (we need a duplex stream, in order to startNextFuture and stop the queue).
  err = snd_seq_open(&hSequencer, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  checkAlsa("open sequencer", err);

  // set our client's name
  err = snd_seq_set_client_name(hSequencer, "a_j_midi-tests");
  checkAlsa("set client name", err);

  clientId = snd_seq_client_id(hSequencer);

  //  // lets create the poll descriptor that we will need when we wait for incoming events.
  //  pollDescriptorsCount = snd_seq_poll_descriptors_count(hSequencer, POLLIN);
  //  pPollDescriptor = (struct pollfd *) alloca(pollDescriptorsCount * sizeof(struct pollfd));
  //  snd_seq_poll_descriptors(hSequencer, pPollDescriptor, pollDescriptorsCount, POLLIN);

  spdlog::trace("Alsa client {} created.", clientId);
}

std::atomic<bool> eventListening{false};
/**
 * Close the ALSA sequencer.
 * This can only be called when event listening has stopped.
 */
void AlsaHelper::closeAlsaSequencer() {
  // make sure the listening queue is stopped for sure.
  if (eventListening) {
    throw std::runtime_error("Receiver cannot be stopped");
  }

  // dirty hack!!!
  // even when "eventListening=false", the "listenForEventsLoop" could access the sequencer for one
  // last time...
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  spdlog::trace("Closing Alsa client {}.", clientId);
  int err = snd_seq_close(hSequencer);
  checkAlsa("snd_seq_close", err);
}

/**
 * create an output (emitting) port.
 * @param portName the name of the port
 * @return a handle for the port.
 */
int AlsaHelper::createOutputPort(const char *portName) {
  int portId; /// the ID-number of our ALSA input port
  portId = snd_seq_create_simple_port(hSequencer, portName,
                                      SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                      SND_SEQ_PORT_TYPE_APPLICATION);
  checkAlsa("createOutputPort", portId);
  return portId;
}

/**
 * create an input (receiving) port.
 * @param portName the name of the port
 * @return a handle (port number) for the new port.
 */
int AlsaHelper::createInputPort(const char *portName) {
  int portId; /// the ID-number of our ALSA input port
  portId = snd_seq_create_simple_port(hSequencer, portName,
                                      SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                      SND_SEQ_PORT_TYPE_APPLICATION);
  checkAlsa("createInputPort", portId);
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

  emitter.client = clientId;
  emitter.port = hEmitterPort;

  receiver.client = clientId;
  receiver.port = hReceiverPort;

  snd_seq_port_subscribe_alloca(&pSubscriptionDescription);
  snd_seq_port_subscribe_set_sender(pSubscriptionDescription, &emitter);
  snd_seq_port_subscribe_set_dest(pSubscriptionDescription, &receiver);
  auto err = snd_seq_subscribe_port(hSequencer, pSubscriptionDescription);
  checkAlsa("connectPorts", err);
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

  receiver.client = clientId;
  receiver.port = hReceiverPort;

  snd_seq_port_subscribe_alloca(&pSubscriptionDescription);
  snd_seq_port_subscribe_set_sender(pSubscriptionDescription, &emitter);
  snd_seq_port_subscribe_set_dest(pSubscriptionDescription, &receiver);
  auto err = snd_seq_subscribe_port(hSequencer, pSubscriptionDescription);
  checkAlsa("connectPorts", err);
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
 * @param interval the time (in milliseconds) to between two "note on" events.
 */
void AlsaHelper::sendEvents(int hEmitterPort, int eventCount, long intervalMs) {
  spdlog::trace("AlsaHelper::sendEvents()");

  long noteOnTime = intervalMs / 2;
  long noteOffTime = intervalMs - noteOnTime;

  snd_seq_event_t evNoteOn;
  snd_seq_ev_set_subs(&evNoteOn);
  snd_seq_ev_set_direct(&evNoteOn);
  snd_seq_ev_set_source(&evNoteOn, hEmitterPort);
  snd_seq_ev_set_noteon(&evNoteOn, 0, 64, 64);

  snd_seq_event_t evNoteOff;
  snd_seq_ev_set_subs(&evNoteOff);
  snd_seq_ev_set_direct(&evNoteOff);
  snd_seq_ev_set_source(&evNoteOff, hEmitterPort);
  snd_seq_ev_set_noteoff(&evNoteOff, 0, 64, 0);

  for (int i = 0; i < eventCount; ++i) {
    auto err = snd_seq_event_output_direct(hSequencer, &evNoteOn);
    checkAlsa("snd_seq_event_output_direct", err);
    err = snd_seq_drain_output(hSequencer);
    checkAlsa("snd_seq_drain_output", err);
    spdlog::trace("          Note on send to output");
    std::this_thread::sleep_for(std::chrono::milliseconds(noteOnTime));

    err = snd_seq_event_output_direct(hSequencer, &evNoteOff);
    checkAlsa("snd_seq_event_output_direct", err);
    err = snd_seq_drain_output(hSequencer);
    checkAlsa("snd_seq_drain_output", err);
    spdlog::trace("          Note off to output");
    std::this_thread::sleep_for(std::chrono::milliseconds(noteOffTime));
  }
}

int AlsaHelper::retrieveEvents() {
  spdlog::trace("AlsaHelper::retrieveEvents()");
  snd_seq_event_t *ev;
  int eventCount = 0;
  int status;

  do {
    status = snd_seq_event_input(hSequencer, &ev);
    switch (status) {
    case -EAGAIN:        // FIFO empty, try again later
    case -ENOSPC:        // // FIFO of sequencer overran, and some events are lost.
      return eventCount; // FIFO of sequencer overran, and some events are lost.
    default:             //
      checkAlsa("snd_seq_event_input", status);
    }

    if (ev) {
      switch (ev->type) {
      case SND_SEQ_EVENT_NOTEON: //
        eventCount++;
        spdlog::trace("          retrieveEvents(Note on)");
        break;
      default: //
        spdlog::trace("          retrieveEvents(other)");
      }
    }
  } while (status > 0);

  return eventCount;
}

int AlsaHelper::listenForEventsLoop(snd_seq_t *pSndSeq) {
  spdlog::trace("AlsaHelper::listenForEventsLoop() - entered");
  int eventCount = 0;

  // lets create the poll descriptors that we will need when we wait for incoming events.
  int fdsCount = snd_seq_poll_descriptors_count(pSndSeq, POLLIN);
  struct pollfd fds[fdsCount];

  while (eventListening) {
    auto err = snd_seq_poll_descriptors(pSndSeq, fds, fdsCount, POLLIN);
    checkAlsa("snd_seq_poll_descriptors", err);
    auto hasEvents = poll(fds, fdsCount, POLL_TIMEOUT_MS);
    if (hasEvents > 0) {
      spdlog::trace("AlsaHelper::listenForEventsLoop() - poll signaled {} event.", hasEvents);
      eventCount = eventCount + retrieveEvents();
    } else {
      spdlog::trace("AlsaHelper::listenForEventsLoop() - poll timed out.");
    }
  }
  return eventCount;
}

FutureEventCount AlsaHelper::startEventReceiver() {
  eventListening = true;
  spdlog::trace("AlsaHelper::startEventReceiver()");
  return std::async(std::launch::async,
                    [pSndSeq = hSequencer]() -> int { return listenForEventsLoop(pSndSeq); });
}

void AlsaHelper::stopEventReceiver(FutureEventCount &future) {
  spdlog::trace("AlsaHelper::stopEventReceiver()");
  // stop the listenForEventsLoop
  eventListening = false;
  // wait until the FutureEventCount has become ready.
  auto status = future.wait_for(std::chrono::milliseconds(2 * POLL_TIMEOUT_MS));
  if (status == std::future_status::timeout) {
    throw std::runtime_error("Receiver cannot be stopped");
  }
}

} // namespace unit_test_helpers