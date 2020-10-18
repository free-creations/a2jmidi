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
#include "sys_clock.h"

#include "alsa_helper.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include <thread>

namespace unitTests {
using namespace unitTestHelpers;

using namespace alsaClient;

// The fixture for testing module AlsaListener.
class AlsaReceiverQueueTest : public ::testing::Test {

protected:
  AlsaReceiverQueueTest() {
    spdlog::set_level(spdlog::level::info);
    SPDLOG_INFO("AlsaReceiverQueueTest-stared");
  }

  ~AlsaReceiverQueueTest() override { SPDLOG_INFO("AlsaReceiverQueueTest-ended"); }

  /**
   * Will be called right before each test.
   */
  void SetUp() override {
    EXPECT_EQ(receiverQueue::getState(), receiverQueue::State::stopped);
    AlsaHelper::openAlsaSequencer();
  }

  /**
   * Will be called immediately after each test.
   */
  void TearDown() override {
    AlsaHelper::closeAlsaSequencer();
    EXPECT_EQ(receiverQueue::getState(), receiverQueue::State::stopped);
    // make sure we don't leak memory.
    EXPECT_EQ(receiverQueue::getCurrentEventBatchCount(), 0);
  }
};

/**
 * An receiverQueue can be started and can be stopped.
 */
TEST_F(AlsaReceiverQueueTest, startStop) {
  namespace queue = receiverQueue; // a shorthand.

  EXPECT_EQ(queue::getState(), queue::State::stopped);

  // auto queueHead{queue::startInternal(AlsaHelper::getSequencerHandle())};
  queue::start(AlsaHelper::getSequencerHandle());
  EXPECT_EQ(queue::getState(), queue::State::running);

  std::this_thread::sleep_for(std::chrono::milliseconds(49));

  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);
}

/**
 * An receiverQueue cannot be started twice.
 */
TEST_F(AlsaReceiverQueueTest, startTwice) {

  namespace queue = receiverQueue; // a shorthand.

  queue::start(AlsaHelper::getSequencerHandle());
  EXPECT_EQ(queue::getState(), queue::State::running);

  std::this_thread::sleep_for(std::chrono::milliseconds(49));

  EXPECT_THROW(queue::start(AlsaHelper::getSequencerHandle());, std::runtime_error);
}

/**
 * A receiverQueue can receive events.
 */
TEST_F(AlsaReceiverQueueTest, receiveEvents) {

  namespace queue = receiverQueue; // a shorthand.

  queue::start(AlsaHelper::getSequencerHandle());
  EXPECT_EQ(queue::getState(), queue::State::running);

  auto emitterPort = AlsaHelper::createOutputPort("out");
  auto receiverPort = AlsaHelper::createInputPort("in");
  AlsaHelper::connectPorts(emitterPort, receiverPort);

  AlsaHelper::sendEvents(emitterPort, 16, 50);

  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);
}

/**
 * A receiverQueue can process the received events.
 */
TEST_F(AlsaReceiverQueueTest, processEvents_1) {

  using namespace std::chrono_literals;
  namespace queue = receiverQueue; // a shorthand.

  queue::start(AlsaHelper::getSequencerHandle());

  auto emitterPort = AlsaHelper::createOutputPort("out");
  auto receiverPort = AlsaHelper::createInputPort("in");
  AlsaHelper::connectPorts(emitterPort, receiverPort);
  constexpr int doubleNoteOns = 4;

  auto startTime = sysClock::now();
  AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto stopTime = sysClock::now() + 1s;

  int noteOnCount = 0;
  queue::process(stopTime, //
                 ([&](const snd_seq_event_t &event, sysClock::TimePoint timeStamp) {
                   // --- the Callback
                   if (event.type == SND_SEQ_EVENT_NOTEON) {
                     noteOnCount++;
                   }
                   EXPECT_GE(timeStamp, startTime);
                   EXPECT_LE(timeStamp, stopTime);
                 }));

  EXPECT_FALSE(queue::hasResult());

  EXPECT_EQ(noteOnCount, doubleNoteOns * 2);
  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);
}

/**
 * A receiverQueue can process the received events.
 */
TEST_F(AlsaReceiverQueueTest, processEvents_2) {

  namespace queue = receiverQueue; // a shorthand.

  queue::start(AlsaHelper::getSequencerHandle());

  auto emitterPort = AlsaHelper::createOutputPort("out");
  auto receiverPort = AlsaHelper::createInputPort("in");
  AlsaHelper::connectPorts(emitterPort, receiverPort);

  // send events in two tranches
  constexpr int doubleNoteOns = 4;
  auto startTime = sysClock::now();
  AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto firstStop = sysClock::now();
  AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto lastStop = sysClock::now();

  // process events of first tranche
  int noteOnCount = 0;
  queue::process(firstStop, //
                 ([&](const snd_seq_event_t &event, sysClock::TimePoint timeStamp) {
                   // --- the Callback
                   if (event.type == SND_SEQ_EVENT_NOTEON) {
                     noteOnCount++;
                   }
                   EXPECT_GE(timeStamp, startTime);
                   EXPECT_LE(timeStamp, firstStop);
                 }));
  // we expect that there are still events remaining.
  EXPECT_TRUE(queue::hasResult());
  EXPECT_EQ(noteOnCount, doubleNoteOns * 2);

  // process events of second tranche
  noteOnCount = 0;
  queue::process(lastStop, //
                 ([&](auto &event, auto timeStamp) {
                   // --- the Callback
                   if (event.type == SND_SEQ_EVENT_NOTEON) {
                     noteOnCount++;
                   }
                   EXPECT_GE(timeStamp, firstStop);
                   EXPECT_LE(timeStamp, lastStop);
                 }));

  EXPECT_EQ(noteOnCount, doubleNoteOns * 2);
  // we expect that there are no events remaining.
  EXPECT_FALSE(queue::hasResult());
  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);
}

/**
 * A receiverQueue can process the received events.
 */
TEST_F(AlsaReceiverQueueTest, processEvents_3) {

  namespace queue = receiverQueue; // a shorthand.

  queue::start(AlsaHelper::getSequencerHandle());

  auto emitterPort = AlsaHelper::createOutputPort("out");
  auto receiverPort = AlsaHelper::createInputPort("in");
  AlsaHelper::connectPorts(emitterPort, receiverPort);

  // send events in a first tranche
  constexpr int doubleNoteOns = 4;
  auto startTime = sysClock::now();
  AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto firstStop = sysClock::now();

  // process all events of the first tranche
  int noteOnCount = 0;
  queue::process(firstStop, //
                 ([&](const snd_seq_event_t &event, sysClock::TimePoint timeStamp) {
                   // --- the Callback
                   if (event.type == SND_SEQ_EVENT_NOTEON) {
                     noteOnCount++;
                   }
                   EXPECT_GE(timeStamp, startTime);
                   EXPECT_LE(timeStamp, firstStop);
                 }));
  // we expect that there are no more events remaining.
  EXPECT_FALSE(queue::hasResult());
  EXPECT_EQ(noteOnCount, doubleNoteOns * 2);

  // refill the queue with events of a second tranche
  auto secondStart = sysClock::now();
  AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto lastStop = sysClock::now();
  // process all events of second tranche
  noteOnCount = 0;
  queue::process(lastStop, //
                 ([&](auto &event, auto timeStamp) {
                   // --- the Callback
                   if (event.type == SND_SEQ_EVENT_NOTEON) {
                     noteOnCount++;
                   }
                   EXPECT_GE(timeStamp, secondStart);
                   EXPECT_LE(timeStamp, lastStop);
                 }));

  EXPECT_EQ(noteOnCount, doubleNoteOns * 2);
  // we expect that there are no events remaining.
  EXPECT_FALSE(queue::hasResult());
  queue::stop();
  EXPECT_EQ(queue::getState(), queue::State::stopped);
}

/**
 *  when calling "process" on a stopped queue, nothing (bad) happens.
 */
TEST_F(AlsaReceiverQueueTest, processStoppedQueue) {

  using namespace std::chrono_literals;

  auto firstStop = sysClock::now() + 2s;

  int callbackCount = 0;
  receiverQueue::process(firstStop, //
                         ([&](const snd_seq_event_t &event, sysClock::TimePoint timeStamp) {
                           // --- the Callback
                           callbackCount++;
                         }));

  EXPECT_EQ(callbackCount, 0);
}

} // namespace unitTests
