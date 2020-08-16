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
#include <stdexcept>

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

/**
 * Open the ALSA sequencer in non-blocking mode.
 */
void AlsaHelper::openAlsaSequencer() {
  int err;
  // open sequencer (we need a duplex stream, in order to start and stop the queue).
  err = snd_seq_open(&_hSequencer, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  checkAlsa("open sequencer", err);

  // set our client's name
  err = snd_seq_set_client_name(_hSequencer, "a_j_midi-tests");
  checkAlsa("set client name", err);

  _clientId =  snd_seq_client_id (_hSequencer);
  spdlog::trace("Alsa client {} created.", _clientId);

}

/**
 * create an output (emitting) port.
 * @param portName the name of the port
 * @return a handle for the port.
 */
int AlsaHelper::createOutputPort(const char *portName) {
  int portId; /// the ID-number of our ALSA input port
  portId = snd_seq_create_simple_port(_hSequencer, portName,
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
  portId = snd_seq_create_simple_port(_hSequencer, portName,
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

  emitter.client = _clientId;
  emitter.port = hEmitterPort;

  receiver.client = _clientId;
  receiver.port = hReceiverPort;

  snd_seq_port_subscribe_alloca(&pSubscriptionDescription);
  snd_seq_port_subscribe_set_sender(pSubscriptionDescription, &emitter);
  snd_seq_port_subscribe_set_dest(pSubscriptionDescription, &receiver);
  auto err = snd_seq_subscribe_port(_hSequencer, pSubscriptionDescription);
  checkAlsa("connectPorts", err);
}

} // namespace unit_test_helpers