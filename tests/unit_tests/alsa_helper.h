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


namespace unit_test_helpers {
/**
 * A quick a dirty interface to ALSA which gives us the means to test.
 */
class AlsaHelper {
private:
  snd_seq_t *_hSequencer; /// handle to access the ALSA sequencer
  int _clientId; /// the client number of this client
public:

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

  /**
   * Open the ALSA sequencer in non-blocking mode.
   */
  void openAlsaSequencer();
  /**
   * create an output (emitting) port.
   * @param portName the name of the port
   * @return a handle (port number) for the new port.
   */
  int createOutputPort(const char *portName);

  /**
   * create an input (receiving) port.
   * @param portName the name of the port
   * @return a handle (port number) for the new port.
   */
  int createInputPort(const char *portName);

  /**
   * Connect an output-port to an input-port. Both ports belong to this client.
   * @param hEmitterPort the port-number of the output-port.
   * @param hReceiverPort the port-number of the input-port.
   */
  void connectPorts(int hEmitterPort, int hReceiverPort);


  /**
   * Sends Midi events through the given emitter port.
   * This call is blocking, control will be given back
   * to the caller once all events have been send.
   * @param hEmitterPort the port-number of the emitter port.
   * @param eventCount the number of events to be send.
   * @param interval the time (in milliseconds) to wait between the sending of two events.
   */
  void sendEvents(int hEmitterPort, int eventCount, long intervalMs);
};
} // namespace unit_test_helpers
#endif //A_J_MIDI_TESTS_UNIT_TESTS_ALSA_HELPER_H
