/*
 * File: alsa_client.h
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
#ifndef A_J_MIDI_SRC_ALSA_CLIENT_H
#define A_J_MIDI_SRC_ALSA_CLIENT_H

#include <string>
namespace alsaClient {
/**
 * Implementation specific stuff.
 */
inline namespace impl {

} // namespace impl

void open(const std::string& deviceName) noexcept(false);
void close() noexcept;

} // namespace alsaClient

#endif // A_J_MIDI_SRC_ALSA_CLIENT_H
