/*
 * File: alsa_util.h
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
#ifndef A_J_MIDI_SRC_ALSA_UTIL_H
#define A_J_MIDI_SRC_ALSA_UTIL_H

#include "spdlog/spdlog.h"

#define ALSA_ERROR(message) SPDLOG_ERROR(message)

namespace alsaClient::util{


void reportError(){
  SPDLOG_ERROR("Some error message with arg: {}", 1);
}

} // namespace alsaClient::util

#endif // A_J_MIDI_SRC_ALSA_UTIL_H
