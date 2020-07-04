/*
 * File: main.cpp
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
#include <boost/program_options.hpp>
#include <iostream>


using namespace std;
using namespace boost;
namespace boostPO = boost::program_options;

/**
 * Program options
 */
#define HELP_OPT "help"
#define VERSION_OPT "version"
#define FAIL_OPT "fail"
#define MULTIPLY_OPT "multiply"
#define DIVIDE_OPT "divide"
#define ARG "argument"

int main(int ac, char *av[]) {
    try {

        boostPO::options_description desc("Allowed options");

        desc.add_options()//
                (HELP_OPT ",h", "display this help and exit")//
                (VERSION_OPT ",v", "display version information and exit")//
                (FAIL_OPT ",x", "exit with non-zero return code")//
                (MULTIPLY_OPT ",m", "multiply arg1 with arg2")//
                (DIVIDE_OPT ",d", "divide arg1 by arg2")//
                (ARG ",a", boostPO::value<vector<int> >(),
                 R"((optional) the arguments for "divide" or "multiply")")//
                ;

        try {
            boostPO::positional_options_description posArgs;
            posArgs.add(ARG, 2);

            boostPO::variables_map varMap;
            boostPO::store(
                    boostPO::command_line_parser(ac, av).options(desc).positional(
                            posArgs).run(),
                    varMap);
            boostPO::notify(varMap);

            if (varMap.count(HELP_OPT)) {
                // print help
                cout << "Usage:  a_j_midi  [options]" << endl;
                cout << desc;
                return 0;
            }

            if (varMap.count(VERSION_OPT)) {
                // print version information
                cout << "Project Name   " << "a_j_midi" << endl << endl;
                cout << "Version Major  " << "0" << endl;
                cout << "Version Minor  " << "0" << endl;
                cout << "Version Patch  " << "0" << endl;
                cout << "Build Type     " << "0" << endl;
                cout << "SVN Revision   " << "0" << endl;
                cout << "Repository URL " << "0" << endl;
                return 0;
            }

            if (varMap.count(FAIL_OPT)) {
                // simulate a program failure
                cout << "sorry world..." << endl;
                return 1;
            }

            if (varMap.count(MULTIPLY_OPT)) {
                vector<int> multiplicants = varMap[ARG].as<vector<int> >();
                if (multiplicants.size() != 2)
                    throw boostPO::error("wrong number of arguments.");

                cout << "Multiplication: " << multiplicants[0] << " x " << multiplicants[1]
                     << " = " << "????" << endl;
            }

            if (varMap.count(DIVIDE_OPT)) {
                vector<int> dividants = varMap[ARG].as<vector<int> >();
                if (dividants.size() != 2)
                    throw boostPO::error("wrong number of arguments.");
                cout << "Division: " << dividants[0] << " / " << dividants[1] << " = "
                     << "????" << endl;
            }

            //That's what the program originally was meant to do.
            cout << "Hello World!!!" << endl;

        }

        catch (boostPO::error &e) {
            cout << "Invalid program options:" << endl;
            cout << "  " << e.what() << endl;
            cout << "Usage:  a_j_midi  [options]" << endl;
            cout << desc;
            return 1;
        }
    }
    catch (std::exception &e) {
        cout << "A problem occurred:" << endl;
        cout << "  " << e.what() << endl;
        return 1;
    }
    return 0;
}