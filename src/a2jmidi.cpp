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
#include "jack_client.h"
#include "spdlog/spdlog.h"
#include <iostream>

namespace a2jmidi {

int start(const std::string &clientNameProposal, const std::string &connectTo, bool startJack) noexcept {
  try {
    jackClient::open(clientNameProposal, startJack);
    const std::string clientName = jackClient::clientName();

    return 0;
  } catch (const std::runtime_error &re) {
    std::cerr << "Runtime error: " << re.what() << std::endl;
  } catch (const std::exception &ex) {
    std::cerr << "Error occurred: " << ex.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown failure occurred." << std::endl;
  }
  return 1;
}

int run(const CommandLineInterpretation &arguments) noexcept {

  switch (arguments.action) {
  case CommandLineAction::messageError:
    std::cout << arguments.message.str();
    return 1;
  case CommandLineAction::messageOK:
    std::cout << arguments.message.str();
    return 0;
  case CommandLineAction::run:
    return start(arguments.clientName, arguments.connectTo, arguments.startJack);
  }
}

} // namespace a2jmidi