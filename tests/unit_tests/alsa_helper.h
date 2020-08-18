/*
 * File: test_helper.h
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
#ifndef A_J_MIDI_TESTS_UNIT_TESTS_ALSA_HELPER_H
#define A_J_MIDI_TESTS_UNIT_TESTS_ALSA_HELPER_H

#include <alsa/asoundlib.h>
#include <future>


namespace unit_test_helpers {

using FutureEventCount = std::future<int>;
/**
 * A quick a dirty interface to ALSA which gives us the means to test.
 */
class AlsaHelper {
private:
  static snd_seq_t *hSequencer; /// handle to access the ALSA sequencer
  static int clientId; /// the client number of this client
  static struct pollfd *pPollDescriptor;
  static int pollDescriptorsCount;
  static constexpr int POLL_TIMEOUT_MS = 10; /// in milliseconds

  int static listenForEventsLoop();
  int static retrieveEvents();


  /**
   * Error handling for ALSA functions.
   * ALSA function often return the error code as a negative result. This function
   * checks the result for negativity.
   * - if negative, it prints the error message and throws a runtime error .
   * - if positive or zero it does nothing.
   * @param operation description of the operation that was attempted.
   * @param alsaResult possible error code from an ALSA call.
   */
  static void checkAlsa(const char *operation, int alsaResult);

public:
  /**
   * Open the ALSA sequencer in non-blocking mode.
   */
  static void openAlsaSequencer();

  /**
   * Close the ALSA sequencer.
   */
  static void closeAlsaSequencer();

  /**
   * create an output (emitting) port.
   * @param portName the name of the port
   * @return a handle (port number) for the new port.
   */
  static int createOutputPort(const char *portName);

  /**
   *
   * @return
   */
  static FutureEventCount startEventReceiver();

  static void stopEventReceiver(FutureEventCount& future);

  /**
   * create an input (receiving) port.
   * @param portName the name of the port
   * @return a handle (port number) for the new port.
   */
  static int createInputPort(const char *portName);

  /**
   * Connect an output-port to an input-port. Both ports belong to this client.
   * @param hEmitterPort the port-number of the output-port.
   * @param hReceiverPort the port-number of the input-port.
   */
  static void connectPorts(int hEmitterPort, int hReceiverPort);

  /**
   * Connect an internal input-port to an external port.
   * Use `$ aconnect -i` to list available ports.
   * @param externalClientId the client id of the external device.
   * @param hExternalPort the port-number of the external port.
   * @param hReceiverPort the port-number of the internal input-port.
   */
  static void connectExternalPort(int externalClientId, int hExternalPort, int hReceiverPort);
  /**
   * Sends Midi events through the given emitter port.
   * This call is blocking, control will be given back
   * to the caller once all events have been send.
   * @param hEmitterPort the port-number of the emitter port.
   * @param eventCount the number of events to be send.
   * @param interval the time (in milliseconds) to wait between the sending of two events.
   */
  static void sendEvents(int hEmitterPort, int eventCount, long intervalMs);
};
} // namespace unit_test_helpers
#endif //A_J_MIDI_TESTS_UNIT_TESTS_ALSA_HELPER_H
