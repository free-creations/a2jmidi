/*
 * File: a2jmidi_main.cpp
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
#include <iostream>

using namespace std;
namespace boostPO = boost::program_options;

#define APPLICATION "a2jmidi"
#define USAGE "Usage:  " APPLICATION "  [options] | [name]"

/**
 * Program options
 */
#define HELP_OPT "help"
#define VERSION_OPT "version"
#define CLIENT_NAME_OPT "name"

int main(int ac, char *av[]) {
  try {
    // declare the supported options
    boostPO::options_description desc("Allowed options");
    desc.add_options()                                             //
        (HELP_OPT ",h", "display this help and exit")              //
        (VERSION_OPT ",v", "display version information and exit") //
        (CLIENT_NAME_OPT ",n", boostPO::value<string>(), "(optional) client name");

    try {
      boostPO::positional_options_description posArgs;
      posArgs.add(CLIENT_NAME_OPT, 1);

      boostPO::variables_map varMap;
      boostPO::store(boostPO::command_line_parser(ac, av).options(desc).positional(posArgs).run(),
                     varMap);
      boostPO::notify(varMap);

      if (varMap.count(HELP_OPT)) {
        // print help
        cout << USAGE << endl;
        cout << desc;
        exit(0);
      }

      if (varMap.count(VERSION_OPT)) {
        // print version information
        cout << APPLICATION << " version " << VERSION << endl;
        exit(0);
      }

      // now lets run the application
      if (varMap.count(CLIENT_NAME_OPT)) {
        a2jmidi::run(varMap[CLIENT_NAME_OPT].as<string>());
      }else {
        a2jmidi::run(APPLICATION);
      }
      exit(0);

    } catch (boostPO::error &e) {
      cout << "Invalid program options:" << endl;
      cout << "  " << e.what() << endl;
      cout << USAGE << endl;
      cout << desc;
      exit(1);
    }
  } catch (std::exception &e) {
    cout << "A problem occurred:" << endl;
    cout << "  " << e.what() << endl;
    exit(1);
  }
}