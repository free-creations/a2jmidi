/*
 * File: alsa_receiver_queue_test.cpp
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

#include "alsa_receiver_queue.h"

#include "alsa_helper.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include <thread>

namespace unitTests {
using namespace unitTestHelpers;

// The fixture for testing module AlsaListener.
class AlsaReceiverQueueTest : public ::testing::Test {

protected:
  AlsaReceiverQueueTest() {
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("AlsaReceiverQueueTest-stared");
  }

  ~AlsaReceiverQueueTest() override { SPDLOG_INFO("AlsaReceiverQueueTest-ended"); }

  /**
   * Will be called right before each test.
   */
  void SetUp() override {
    EXPECT_EQ(alsaReceiverQueue::getState(), alsaReceiverQueue::State::stopped);
    AlsaHelper::openAlsaSequencer();
  }

  /**
   * Will be called immediately after each test.
   */
  void TearDown() override {
    AlsaHelper::closeAlsaSequencer();
    EXPECT_EQ(alsaReceiverQueue::getState(), alsaReceiverQueue::State::stopped);
    // make sure we don't leak memory.
    EXPECT_EQ(alsaReceiverQueue::getCurrentEventCount(), 0);
  }
};

/**
 * An alsaReceiverQueue can be started and can be stopped.
 */
TEST_F(AlsaReceiverQueueTest, startStop) {
  namespace queue = alsaReceiverQueue;

  EXPECT_EQ(queue::getState(), queue::State::stopped);

  auto queueHead{queue::start(AlsaHelper::getSequencerHandle())};
  EXPECT_EQ(queue::getState(), queue::State::running);

  std::this_thread::sleep_for(std::chrono::milliseconds(49));

  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);

  EXPECT_TRUE(isReady(queueHead));
}

/**
 * An alsaReceiverQueue cannot be started twice.
 */
TEST_F(AlsaReceiverQueueTest, startTwice) {

  namespace queue = alsaReceiverQueue;

  auto queueHead1{queue::start(AlsaHelper::getSequencerHandle())};
  EXPECT_EQ(queue::getState(), queue::State::running);

  std::this_thread::sleep_for(std::chrono::milliseconds(49));

  EXPECT_THROW(auto invalidQueue{queue::start(AlsaHelper::getSequencerHandle())};,std::runtime_error);
}
} // namespace unitTests
