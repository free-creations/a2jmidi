/*
 * File: alsa_helper_test.cpp
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


  AlsaHelperTest() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("AlsaHelperTest: start");
  }

  ~AlsaHelperTest() override {
    spdlog::info("AlsaHelperTest: end");
  }

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

  std::this_thread::sleep_for(std::chrono::milliseconds(std::lround(2.5 * AlsaHelper::POLL_TIMEOUT_MS)));

  AlsaHelper::stopEventReceiver(futureEventCount);

  auto eventCount = futureEventCount.get();
  EXPECT_EQ(eventCount, 0);

}

/**
 * The AlsaHelper can emit events.
 */
TEST_F(AlsaHelperTest, sendEvents) {

  auto hEmitterPort = AlsaHelper::createOutputPort("output");

  // int eventCount = 2*4*60; // Emit during two minutes (enough time to connect the sequencer).
  int eventCount = 3; // for automatic testing
  // Send events four Notes per second (240 BPM).
  AlsaHelper::sendEvents(hEmitterPort, eventCount, 250);
}

/**
 * The Receiver of the AlsaHelper can receive events.
 */
TEST_F(AlsaHelperTest, receiveEvents) {

  auto futureEventCount = AlsaHelper::startEventReceiver();
  AlsaHelper::createInputPort("input");


  //long listeningTimeMs = 2*60*1000;  // enough time to manually connect keyboard or sequencer).
  long listeningTimeMs = 2;  // for automatic test
  std::this_thread::sleep_for(std::chrono::milliseconds (listeningTimeMs));
  AlsaHelper::stopEventReceiver(futureEventCount);

}

/**
 * The Receiver of the AlsaHelper can send and receive events.
 */
TEST_F(AlsaHelperTest, sendReceiveEvents) {

  auto futureEventCount = AlsaHelper::startEventReceiver();

  auto hReceiverPort = AlsaHelper::createInputPort("input");
  auto hEmitterPort = AlsaHelper::createOutputPort("output");
  AlsaHelper::connectPorts(hEmitterPort, hReceiverPort);

  int eventsEmitted = 7;
  AlsaHelper::sendEvents(hEmitterPort, eventsEmitted, 250);

  AlsaHelper::stopEventReceiver(futureEventCount);
  auto eventsReceived = futureEventCount.get();

  EXPECT_EQ(eventsEmitted, eventsReceived);

}
} // namespace unit_test_helpers
