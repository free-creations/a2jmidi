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
  namespace queue = alsaReceiverQueue; // a shorthand.

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

  namespace queue = alsaReceiverQueue; // a shorthand.

  auto queueHead1{queue::start(AlsaHelper::getSequencerHandle())};
  EXPECT_EQ(queue::getState(), queue::State::running);

  std::this_thread::sleep_for(std::chrono::milliseconds(49));

  EXPECT_THROW(auto invalidQueue{queue::start(AlsaHelper::getSequencerHandle())};
               , std::runtime_error);
}

/**
 * An alsaReceiverQueue can receive events.
 */
TEST_F(AlsaReceiverQueueTest, receiveEvents) {

  namespace queue = alsaReceiverQueue; // a shorthand.

  auto queueHead{queue::start(AlsaHelper::getSequencerHandle())};
  EXPECT_EQ(queue::getState(), queue::State::running);

  auto emitterPort = AlsaHelper::createOutputPort("out");
  auto receiverPort = AlsaHelper::createInputPort("in");
  AlsaHelper::connectPorts(emitterPort, receiverPort);

  AlsaHelper::sendEvents(emitterPort, 16, 50);

  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);

  EXPECT_TRUE(isReady(queueHead));
}

/**
 * An alsaReceiverQueue can process the received events.
 */
TEST_F(AlsaReceiverQueueTest, processEvents) {

  namespace queue = alsaReceiverQueue; // a shorthand.

  auto queueHead{queue::start(AlsaHelper::getSequencerHandle())};

  auto emitterPort = AlsaHelper::createOutputPort("out");
  auto receiverPort = AlsaHelper::createInputPort("in");
  AlsaHelper::connectPorts(emitterPort, receiverPort);
  constexpr int doubleNoteOns = 4;
  AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto firstStop = queue::Sys_clock::now();
  AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);


  int noteOnCount = 0;
  queueHead = queue::forEach( //
      std::move(queueHead), firstStop,
      ([&](const snd_seq_event_t &event, queue::TimePoint timeStamp) {
        // --- forEachCallback
        switch (event.type) {
        case SND_SEQ_EVENT_NOTEON: //
          noteOnCount++;
          break;
        }
      }));
  EXPECT_TRUE(isReady(queueHead));
//  EXPECT_EQ(noteOnCount, doubleNoteOns * 2);

//  noteOnCount = 0;
//  queueHead = queue::forEach( //
//      std::move(queueHead), queue::Sys_clock::now(),
//      ([&](const snd_seq_event_t &event, queue::TimePoint timeStamp) {
//        // --- forEachCallback
//        switch (event.type) {
//        case SND_SEQ_EVENT_NOTEON: //
//          noteOnCount++;
//          break;
//        }
//      }));
//  EXPECT_EQ(noteOnCount, doubleNoteOns * 2);

  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);


}

} // namespace unitTests
