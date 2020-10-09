/*
 * File: a2jmidi_commandLineParser.cpp
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
#include "version.h"
#include <boost/program_options.hpp>


using namespace std;
namespace boostPO = boost::program_options;


#define USAGE "Usage:  " APPLICATION "  [options] | [name]"

namespace a2jmidi {

/**
 * Program options
 */
#define HELP_OPT "help"
#define VERSION_OPT "version"
#define CLIENT_NAME_OPT "name"
#define NO_START_SERVER_OPT "noStartServer"

/**
 * Interpret the instructions given by the user on the commend line.
 * @param ac - number of tokens in the command line, plus one
 * @param av - the tokens given by the user
 * @return whatever follows from interpreting the command line.
 */
CommandLineInterpretation parseCommandLine(int ac, const char *av[]) {
  CommandLineInterpretation result;

  try {
    // declare the supported options
    boostPO::options_description desc("Allowed options");
    desc.add_options()                                             //
        (HELP_OPT ",h", "display this help and exit")              //
        (VERSION_OPT ",v", "display version information and exit") //
        (NO_START_SERVER_OPT ",s", "Do not attempt to automatically start the JACK server") //
        (CLIENT_NAME_OPT ",n", boostPO::value<string>(), "(optional) client name");

    try {
      boostPO::positional_options_description posArgs;
      posArgs.add(CLIENT_NAME_OPT, 1);

      boostPO::variables_map varMap;
      boostPO::store(boostPO::command_line_parser(ac, av).options(desc).positional(posArgs).run(),
                     varMap);
      boostPO::notify(varMap);

      if (varMap.count(HELP_OPT)) {
        // print help and exit
        result.message << USAGE << endl;
        result.message << desc;
        result.action = CommandLineAction::messageOK;
        return result;
      }

      if (varMap.count(VERSION_OPT)) {
        // print version information
        result.message << APPLICATION << " version " << VERSION << endl;
        result.action = CommandLineAction::messageOK;
        return result;
      }

      if (varMap.count(NO_START_SERVER_OPT)) {
        // set the noStartServerOption
        result.noStartServer = true;
      }

      // now lets run the application
      if (varMap.count(CLIENT_NAME_OPT)) {
        result.clientName = varMap[CLIENT_NAME_OPT].as<string>();
      } else {
        result.clientName = APPLICATION;
      }
      result.action = CommandLineAction::run;
      return result;

    } catch (boostPO::error &e) {
      result.message << "Invalid program options:" << endl;
      result.message << "  " << e.what() << endl;
      result.message << USAGE << endl;
      result.message << desc;
      result.action = CommandLineAction::messageError;
      return result;
    }
  } catch (std::exception &e) {
    result.message << "A problem occurred:" << endl;
    result.message << "  " << e.what() << endl;
    result.action = CommandLineAction::messageError;
    return result;
  }
}
} // namespace a2jmidi