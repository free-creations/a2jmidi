/*
 * File: a2jmidi.cpp
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
#include "a2jmidi.h"
#include "spdlog/spdlog.h"

namespace a2jmidi {

void run(const CommandLineInterpretation &arguments) noexcept{
  spdlog::set_level(spdlog::level::trace);
  SPDLOG_INFO("a2jmidi::run - device name: {}",arguments.clientName);
}

} // namespace a2jmidi