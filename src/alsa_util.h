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
#include <alsa/asoundlib.h>


/**
 * Error handling for ALSA functions.
 * ALSA function often return the error code as a negative result. This function
 * checks the result for negativity.
 *
 * - if negative, it prints the error message and returns true (= yes, there is an error).
 * - if positive or zero it does nothing and returns false (= no, there is no error).
 *
 * @param alsaResult - the result from an ALSA call.
 * @param operation - description of the operation that was attempted.
 * @return true if the alsaResult shows an error, false if the alsaResult is OK.
 */
#define ALSA_ERROR(alsaResult, operation)                                                         \
  (alsaClient::util::error(alsaResult, operation, __FILE__, __LINE__, SPDLOG_FUNCTION))

namespace alsaClient::util {

/**
 * Error handling for ALSA functions.
 * ALSA function often return the error code as a negative result. This function
 * checks the result for negativity.
 *
 * - if negative, it prints the error message and returns true (= yes, there is an error).
 * - if positive or zero it does nothing and returns false (= no, there is no error).
 *
 * @param alsaResult - the result from an ALSA call.
 * @param operation - description of the operation that was attempted.
 * @param sourceFile - name of sourcefile where the call is coded.
 * @param sourceLine - the line-number in sourcefile where the call is coded.
 * @param sourceFunction - name of the function or procedure where the call is coded.
 * @return true if the alsaResult shows an error, false if the alsaResult is OK.
 */
bool error(int alsaResult, const char *operation, const char *sourceFile, int sourceLine,
           const char *sourceFunction) {
  if (alsaResult < 0) {
    constexpr int buffSize = 256;
    char buffer[buffSize];
    snprintf(buffer, buffSize, "ALSA cannot %s - %s", operation, snd_strerror(alsaResult));
    spdlog::source_loc sourceLoc{sourceFile, sourceLine, sourceFunction};
    spdlog::default_logger_raw()->log(sourceLoc, spdlog::level::err, buffer);
    return true;
  }
  return false;
}

} // namespace alsaClient::util

#endif // A_J_MIDI_SRC_ALSA_UTIL_H
