/*
 * File: alsa_client_test.cpp
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

#include "alsa_client.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include <thread>

#include "alsa_helper.h"
#include "gmock/gmock.h"
// #include <thread> // when waiting for the console (see createPortSillyNames).

namespace unitTests {
/***
 * Testing the module `AlsaClient`.
 */
class AlsaClientTest : public ::testing::Test {

protected:
  AlsaClientTest() {
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("AlsaClientTest-stared");
  }

  ~AlsaClientTest() override { SPDLOG_INFO("AlsaClientTest-ended"); }
};

/**
 * we can `open` and `close` the AlsaClient.
 */
TEST_F(AlsaClientTest, openClose) {
  alsaClient::open("unitTestAlsaDevice");
  EXPECT_EQ(alsaClient::clientName(), "unitTestAlsaDevice");
  alsaClient::close();
}
/**
 * we can create a port.
 */
TEST_F(AlsaClientTest, createPort) {
  alsaClient::open("unitTestAlsaDevice");

  alsaClient::newReceiverPort("testPort");
  EXPECT_EQ(alsaClient::portName(), "testPort");

  alsaClient::close();
}
/**
 * we can create a port ... and connect it an existing sender port.
 */
TEST_F(AlsaClientTest, createPortAndConnect) {
  using namespace std::chrono_literals;
  alsaClient::open("unitTestAlsaDevice");

  alsaClient::newReceiverPort("unitTestAlsaDevice Port-0", "Midi Through Port-0");
  // std::this_thread::sleep_for(30s); // time to run `aconnect -l' in the console

  auto portIds = alsaClient::receiverPortGetConnections();
  EXPECT_FALSE(portIds.empty());

  SPDLOG_TRACE("createPortAndConnect - connected to \"Midi Through Port-0\" [{}:{}]",
               portIds[0].client, portIds[0].port);

  alsaClient::close();
}
/**
 * When using a completely silly names (for example nothing but blanks), ALSA will use these without
 * moaning. Use `$ aconnect -o` to check.
 */
TEST_F(AlsaClientTest, createPortSillyNames) {
  using namespace std::chrono_literals;
  alsaClient::open("        ");
  EXPECT_EQ(alsaClient::clientName(), "        ");

  alsaClient::newReceiverPort("        ");
  EXPECT_EQ(alsaClient::portName(), "        ");

  // std::this_thread::sleep_for(30s); // time to run `aconnect -o' in the console

  alsaClient::close();
}

/**
 * When using empty names, ALSA will replace these with something like `Client-128:port-0`.
 * Use `$ aconnect -o` to check.
 */
TEST_F(AlsaClientTest, createPortEmptyNames) {
  using namespace std::chrono_literals;
  using ::testing::StartsWith; // google-mock `StartsWith` matcher

  alsaClient::open("");
  EXPECT_THAT(alsaClient::clientName(), StartsWith("Client-"));

  alsaClient::newReceiverPort("");
  EXPECT_EQ(alsaClient::portName(), "port-0");

  // std::this_thread::sleep_for(30s); // time to run `aconnect -o' in the console
  alsaClient::close();
}

/**
 * The receiverQueue can be started and can be stopped.
 */
TEST_F(AlsaClientTest, startStop) {
  EXPECT_EQ(alsaClient::state(), alsaClient::State::closed);

  alsaClient::open("unitTestAlsaDevice");
  EXPECT_EQ(alsaClient::state(), alsaClient::State::idle);

  alsaClient::activate();
  EXPECT_EQ(alsaClient::state(), alsaClient::State::running);

  alsaClient::stop();
  EXPECT_EQ(alsaClient::state(), alsaClient::State::idle);

  alsaClient::close();
  EXPECT_EQ(alsaClient::state(), alsaClient::State::closed);
}

/**
 * The receiverQueue can receive events.
 */
TEST_F(AlsaClientTest, processEvents) {

  using namespace std::chrono_literals;
  unitTestHelpers::AlsaHelper::openAlsaSequencer("sender");
  auto emitterPort = unitTestHelpers::AlsaHelper::createOutputPort("port");

  alsaClient::open("testClient");
  alsaClient::newReceiverPort("testPort", "sender:port");
  auto startTime = sysClock::now();
  alsaClient::activate();
  ASSERT_EQ(alsaClient::state(), alsaClient::State::running);

  constexpr int doubleNoteOns = 4;
  unitTestHelpers::AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto stopTime = sysClock::now()+1ms;

  // std::this_thread::sleep_for(300s); // time to run `aconnect -o' in the console

  int noteCount = 0;

  auto processMidi = [&](const midi::Event &event, sysClock::TimePoint timeStamp) -> int {
    noteCount++;
    EXPECT_EQ(event.size(),3);//Note on and note off are three byte size.
    EXPECT_GE(timeStamp, startTime);
    EXPECT_LE(timeStamp, stopTime);
    return 0;
  };

  int err = alsaClient::retrieve(stopTime, processMidi);

  EXPECT_FALSE(err);
  EXPECT_EQ(noteCount, 16);
  alsaClient::close();
  unitTestHelpers::AlsaHelper::closeAlsaSequencer();
}
/**
 * the receiver queue is not processed after an error has been signaled by the forEachClosure
 */
TEST_F(AlsaClientTest, processEventsError) {

  using namespace std::chrono_literals;
  unitTestHelpers::AlsaHelper::openAlsaSequencer("sender");
  auto emitterPort = unitTestHelpers::AlsaHelper::createOutputPort("port");

  alsaClient::open("testClient");
  alsaClient::newReceiverPort("testPort", "sender:port");
  auto startTime = sysClock::now();
  alsaClient::activate();

  constexpr int doubleNoteOns = 4;
  unitTestHelpers::AlsaHelper::sendEvents(emitterPort, doubleNoteOns, 50);
  auto stopTime = sysClock::now()+1ms;

  int invocationCount = 0;

  auto forEachClosure = [&](const midi::Event &event, sysClock::TimePoint timeStamp) -> int {
    invocationCount++;
    return 1; // << signal error here
  };

  int err = alsaClient::retrieve(stopTime, forEachClosure);

  EXPECT_EQ(invocationCount, 1);
  EXPECT_TRUE(err);
  alsaClient::close();
  unitTestHelpers::AlsaHelper::closeAlsaSequencer();
}
} // namespace unitTests
