/*
 * File: jack_client_test_no_server.cpp
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

#include "jack_client.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include <cstdlib>
#include <thread>

namespace unitTests {
/***
 * Testing the module `jackClient`.
 *
 * This test suite regroups all the test
 * that require the JACK server to be down on start.
 */
class JackClientTestNoServer : public ::testing::Test {

protected:
  JackClientTestNoServer() {
    // make sure that the JACK server is down.
    int err = system("jack_control stop");
    EXPECT_EQ(err, 0);
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("JackClientTestNoServer-stared - jack_control returned {}", err);
  }

  ~JackClientTestNoServer() override { SPDLOG_INFO("JackClientTestNoServer-ended"); }
};

/**
 * we can `open` and `close` the JackClient, even when the server is not started.
 */
TEST_F(JackClientTestNoServer, openClose_startServer) {
  EXPECT_EQ(jackClient::state(), jackClient::State::closed);
  jackClient::open("UnitTestClient", false);
  EXPECT_EQ(jackClient::state(), jackClient::State::opened);
  jackClient::close();
  EXPECT_EQ(jackClient::state(), jackClient::State::closed);
}

/**
 * when the server is not started and we try to open with option `noStartServer`
 * we'll fail on an exception.
 */
TEST_F(JackClientTestNoServer, openClose_noStartServer) {
  EXPECT_EQ(jackClient::state(), jackClient::State::closed);

  EXPECT_THROW(jackClient::open("UnitTestClient", true), //
               jackClient::ServerNotRunningException);

  EXPECT_EQ(jackClient::state(), jackClient::State::closed);
}

} // namespace unitTests
