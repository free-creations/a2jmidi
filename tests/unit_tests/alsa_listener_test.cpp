/*
 * File: alsa_listener_test.cpp
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

#include "alsa_listener.h"

#include "alsa_helper.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include <thread>


namespace unit_test {
using namespace unit_test_helpers;

// The fixture for testing module AlsaListener.
class AlsaListenerTest : public ::testing::Test {

protected:
 

  AlsaListenerTest() {
    spdlog::set_level(spdlog::level::trace); // Set global log level
  }

  ~AlsaListenerTest() override = default;

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  /**
   * Will be called right before each test.
   */
  void SetUp() override {
    AlsaHelper::openAlsaSequencer();
  }

  /**
   * Will be called immediately after each test.
   */
  void TearDown() override {
    AlsaHelper::closeAlsaSequencer();
  }




};

/**
 * an alsa-event-chain can be started and can be stopped.
 */
TEST_F(AlsaListenerTest, startStopEventChain) {

  auto pEventChain{alsa_listener::startFuture(0)};
  alsa_listener::terminateListening();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  ASSERT_TRUE(alsa_listener::isReady(pEventChain));
}


} // namespace unit_test
