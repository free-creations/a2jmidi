/*
 * File: test_helper.cpp
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

snd_seq_t * AlsaHelper::hSequencer{nullptr}; /// handle to access the ALSA sequencer
int AlsaHelper::clientId{0}; /// the client-number of this client
struct pollfd * AlsaHelper::pPollDescriptor{nullptr};
int AlsaHelper::pollDescriptorsCount{0};

/**
 * Open the ALSA sequencer in non-blocking mode.
 */
void AlsaHelper::openAlsaSequencer() {
  int err;
  // open sequencer (we need a duplex stream, in order to start and stop the queue).
  err = snd_seq_open(&hSequencer, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  checkAlsa("open sequencer", err);

  // set our client's name
  err = snd_seq_set_client_name(hSequencer, "a_j_midi-tests");
  checkAlsa("set client name", err);

  clientId = snd_seq_client_id(hSequencer);

  // lets create the poll descriptor that we will need when we wait for incoming events.
  pollDescriptorsCount = snd_seq_poll_descriptors_count(hSequencer, POLLIN);
  pPollDescriptor = (struct pollfd *) alloca(pollDescriptorsCount * sizeof(struct pollfd));
  snd_seq_poll_descriptors(hSequencer, pPollDescriptor, pollDescriptorsCount, POLLIN);

  spdlog::trace("Alsa client {} created.", clientId);
}

std::atomic<bool> eventListening{false};
/**
 * Close the ALSA sequencer.
 * This can only be called when event listening has stopped.
 */
void AlsaHelper::closeAlsaSequencer() {
  // make sure the listening queue is stopped for sure.
  if(eventListening){
    throw std::runtime_error("Receiver cannot be stopped");
  }

  spdlog::trace("Closing Alsa client {}.", clientId);
  int err = snd_seq_close 	(hSequencer);
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
 * Sends Midi events through the given emitter port.
 *
 * This call is blocking. That means, control will be given back
 * to the caller once all events have been send.
 * @param hEmitterPort the port-number of the emitter port.
 * @param eventCount the number of events to be send.
 * @param interval the time (in milliseconds) to wait between the sending of two events.
 */
void AlsaHelper::sendEvents(int hEmitterPort, int eventCount, long intervalMs) {

  snd_seq_event_t ev;
  snd_seq_ev_set_direct(&ev);
  snd_seq_ev_set_fixed(&ev);
  snd_seq_ev_set_source(&ev, hEmitterPort);

  ev.type = SND_SEQ_EVENT_NOTEON;
  ev.data.note.channel = 1;
  ev.data.note.velocity = 0;

  for (int i = 0; i < eventCount; ++i) {
    ev.data.note.note = i%128;
    snd_seq_event_output_direct(hSequencer, &ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
  }
}




int AlsaHelper::retrieveEvents(){

  snd_seq_event_t *ev;
  int eventCount = 0;

  while(snd_seq_event_input(hSequencer, &ev)>0){
    spdlog::trace("AlsaHelper::retrieveEvents()");

    switch (ev->type) {
    case SND_SEQ_EVENT_NOTEON:
      eventCount++;
      break;
    }
    snd_seq_free_event(ev);
  }
  return eventCount;
}

int AlsaHelper::listenForEventsLoop(){
  int eventCount = 0;
  while(eventListening) {
    auto hasEvents = poll(pPollDescriptor, pollDescriptorsCount, POLL_TIMEOUT_MS);
    if (hasEvents > 0) {
      eventCount = eventCount + retrieveEvents();
    }
  }
  return eventCount;
}

FutureEventCount AlsaHelper::startEventReceiver() {
  eventListening = true;
  spdlog::trace("AlsaHelper::startEventReceiver()");
  return std::async(std::launch::async, []() -> int {
    return listenForEventsLoop();
  });
}

void AlsaHelper::stopEventReceiver(FutureEventCount& future) {
  spdlog::trace("AlsaHelper::stopEventReceiver()");
  // stop the listenForEventsLoop
  eventListening = false;
  // wait until the FutureEventCount has become ready.
  auto status = future.wait_for(std::chrono::milliseconds (2*POLL_TIMEOUT_MS));
  if(status == std::future_status::timeout){
    throw std::runtime_error("Receiver cannot be stopped");
  }
}

} // namespace unit_test_helpers