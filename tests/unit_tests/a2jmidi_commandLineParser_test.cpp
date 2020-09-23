/*
 * File: a2jmidi_commandLineParser_test.cpp
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
#include "gtest/gtest.h"

namespace unitTests {
class A2jmidiCommandLineParserTest : public ::testing::Test {
protected:
  A2jmidiCommandLineParserTest() {
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("A2jmidiCommandLineParserTest-stared");
  }

  ~A2jmidiCommandLineParserTest() override { SPDLOG_INFO("A2jmidiCommandLineParserTest-ended"); }
};

/**
 * when called with no arguments, all arguments are defaulted and the application is executed.
 */
TEST_F(A2jmidiCommandLineParserTest, noArguments) {
  using namespace a2jmidi;
  constexpr int parmCount = 1 + 0;

  // the program arguments (first arg is always full path to the program executed)
  const char *av[parmCount] = {"./a2jmidi"};

  CommandLineArguments result = parseCommandLine(parmCount, av);
  EXPECT_EQ(result.clientName, "a2jmidi");
  EXPECT_EQ(result.action, CommandLineAction::run);
}
/**
 * when called with an invalid argument, an error message is printed and the application exits.
 */
TEST_F(A2jmidiCommandLineParserTest, invalidArguments) {
  using namespace a2jmidi;
  constexpr int parmCount = 1 + 1;

  // the program arguments (first arg is always full path to the program executed)
  const char *av[parmCount] = {"./a2jmidi", "--strangeOption"};

  CommandLineArguments result = parseCommandLine(parmCount, av);
  SPDLOG_TRACE("Error message: \n{}", result.message.str());

  EXPECT_EQ(result.action, CommandLineAction::messageError);
}

/**
 * when called with one argument, this argument is the client name.
 */
TEST_F(A2jmidiCommandLineParserTest, singleArgument) {
  using namespace a2jmidi;
  constexpr int parmCount = 1 + 1;

  // the program arguments (first arg is always full path to the program executed)
  const char *av[parmCount] = {"./a2jmidi", "client_name"};

  CommandLineArguments result = parseCommandLine(parmCount, av);
  EXPECT_EQ(result.clientName, "client_name");
  EXPECT_EQ(result.action, CommandLineAction::run);
}

/**
 *  --help will display help.
 */
TEST_F(A2jmidiCommandLineParserTest, helpOption) {
  using namespace a2jmidi;
  constexpr int parmCount = 1 + 1;

  // the program arguments (first arg is always full path to the program executed)
  const char *av[parmCount] = {"./a2jmidi", "--help"};

  CommandLineArguments result = parseCommandLine(parmCount, av);
  SPDLOG_TRACE("Help message: \n{}", result.message.str());
  EXPECT_EQ(result.action, CommandLineAction::messageOK);
}
/**
 *  --version will display the version.
 */
TEST_F(A2jmidiCommandLineParserTest, versionOption) {
  using namespace a2jmidi;
  constexpr int parmCount = 1 + 1;

  // the program arguments (first arg is always full path to the program executed)
  const char *av[parmCount] = {"./a2jmidi", "--version"};

  CommandLineArguments result = parseCommandLine(parmCount, av);
  SPDLOG_TRACE("Help message: \n{}", result.message.str());
  EXPECT_EQ(result.action, CommandLineAction::messageOK);
}
} // namespace unitTests
