/*
 * File: a2jmidi.h
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
#ifndef A_J_MIDI_SRC_A2JMIDI_H
#define A_J_MIDI_SRC_A2JMIDI_H

#include <sstream>
#include <string>

#define APPLICATION "a2jmidi"

namespace a2jmidi {
inline namespace impl {
std::string open(const std::string &name) noexcept;
void shutdown() noexcept;
} // namespace impl

/**
 * The command line action indicates what the program should do after having interpreted the command
 * line.
 */
enum class CommandLineAction : int {
  messageError, /// show message and exit on error (the given Command Line could not be parsed)
  messageOK,    /// only show message and exit without error (show version, show help etc.)
  run           /// start running with the given arguments.
};

/**
 * The result of parsing the command line.
 */
struct CommandLineInterpretation {
public:
  CommandLineInterpretation() = default;
  CommandLineInterpretation(const CommandLineInterpretation &) = delete; /// no copy constructor
  CommandLineInterpretation(CommandLineInterpretation &&) = default; /// default move constructor
  std::stringstream message;                                         /// a message to display
  CommandLineAction action{CommandLineAction::run};                  /// what shall the app do
  std::string clientName{APPLICATION};                               /// a proposed device name
  std::string connectTo;   /// name of a port to connect to
  bool startJack{false}; /// should the JACK server be started
};

/**
 * Interpret the instructions given by the user on the commend line.
 * @param ac - number of tokens in the command line, plus one
 * @param av - the tokens given by the user
 * @return whatever follows from interpreting the command line.
 */
CommandLineInterpretation parseCommandLine(int ac, const char *av[]);

void run(const CommandLineInterpretation &arguments) noexcept;

} // namespace a2jmidi
#endif // A_J_MIDI_SRC_A2JMIDI_H
