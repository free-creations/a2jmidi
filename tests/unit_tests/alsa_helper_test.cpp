/*
 * File: clock_test.cpp
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



#include "alsa_helper.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include <thread>

using testing::Ge;
namespace unit_test_helpers {
// The fixture for testing class AlsaHelper.
class AlsaHelperTest : public ::testing::Test {

protected:
  //AlsaHelper alsaHelper{};

  AlsaHelperTest() {
    spdlog::set_level(spdlog::level::trace); // Set global log level
  }

  ~AlsaHelperTest() override = default;

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

  // Class members declared here can be used by all tests in the test suite
  // for Clock.
};

/**
 * The AlsaSequencer can be opened and can be  closed.
 */
TEST_F(AlsaHelperTest, openCloseAlsaSequencer) {
  // just let SetUp() and TearDown() do the work
}

/**
 * The Receiver of the AlsaHelper can be started and stopped.
 */
TEST_F(AlsaHelperTest, startStopEventReceiver) {

  auto futureEventCount = AlsaHelper::startEventReceiver();

  AlsaHelper::stopEventReceiver(futureEventCount);
  auto eventCount = futureEventCount.get();

  EXPECT_EQ(eventCount, 0);

}

/**
 * The Receiver of the AlsaHelper can receive events.
 */
TEST_F(AlsaHelperTest, listenOnInputPort) {

  auto futureEventCount = AlsaHelper::startEventReceiver();

  auto portId = AlsaHelper::createInputPort("input");

  AlsaHelper::connectExternalPort(28,0,portId);

  //AlsaHelper::stopEventReceiver(futureEventCount);
  //auto eventCount = futureEventCount.get();

}
} // namespace unit_test_helpers
