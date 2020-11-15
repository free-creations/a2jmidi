/*
 * File: jack_client_test.cpp
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

#include "a2jmidi_clock.h"
#include "jack_client.h"
#include "spdlog/spdlog.h"
#include "sys_clock.h"
#include <chrono>
#include <climits>
#include <cstdlib>
#include "gtest/gtest.h"
#include <thread>

namespace unitTests {
/***
 * Testing the module `jackClient`.
 * This test suite regroups all the test that require a running JACK server.
 */
class JackClientTest : public ::testing::Test {

protected:
  JackClientTest() {
    // start jack server if not currently started
    int err = system("jack_control start");
    EXPECT_EQ(err, 0);
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("JackClientTest-stared - jack_control returned {}", err);
  }

  ~JackClientTest() override { SPDLOG_INFO("JackClientTest-ended"); }
  /**
   * Will be called right before each test.
   */
  void SetUp() override {
    EXPECT_EQ(jackClient::state(), jackClient::State::closed);
    jackClient::open("UnitTestClient", true);
    EXPECT_EQ(jackClient::state(), jackClient::State::idle);
  }

  /**
   * Will be called immediately after each test.
   */
  void TearDown() override {
    jackClient::close();
    EXPECT_EQ(jackClient::state(), jackClient::State::closed);
  }
};

/**
 * provided the jack server is running,
 * we can `open` and `close` the JackClient.
 */
TEST_F(JackClientTest, openClose) {
  // test moved to SetUp() and  TearDown();
}
/**
 * provided the client is open,
 * we can create a sender port.
 */
TEST_F(JackClientTest, createPort) {
  auto *port = jackClient::newSenderPort("port");
  EXPECT_NE(port, nullptr);
}

/**
 * provided the client is open,
 * we can `activate` and `stop` the JackClient.
 */
TEST_F(JackClientTest, activateStop) {
  jackClient::activate();
  EXPECT_EQ(jackClient::state(), jackClient::State::running);

  jackClient::stop();
  EXPECT_EQ(jackClient::state(), jackClient::State::idle);
}

/**
 * provided the client is open,
 * we can install a callback.
 */
TEST_F(JackClientTest, callback) {
  using namespace std::chrono_literals;
  int callbackCount = 0;

  jackClient::registerProcessCallback(([&](int nFrames, sysClock::TimePoint deadLine) -> int {
    EXPECT_LE(deadLine, sysClock::now());
    callbackCount++;
    return 0;
  }));

  jackClient::activate();
  EXPECT_EQ(jackClient::state(), jackClient::State::running);

  std::this_thread::sleep_for(500ms);
  jackClient::stop();
  EXPECT_GT(callbackCount, 0);
  EXPECT_EQ(jackClient::state(), jackClient::State::idle);
}
/**
 * There should be very few timing resets needed.
 */
TEST_F(JackClientTest, stableTiming) {
  using namespace std::chrono_literals;

  jackClient::registerProcessCallback( //
      [&](int nFrames, sysClock::TimePoint deadLine) -> int {
        EXPECT_LE(deadLine, sysClock::now());
        return 0;
      });
  jackClient::activate();
  std::this_thread::sleep_for(50ms);
  // on startup we'll accept some hick ups.
  EXPECT_LE(jackClient::impl::g_resetTimingCount, 3);

  // now let it run for one second
  std::this_thread::sleep_for(1000ms);
  jackClient::stop();

  // there should not be any more resets...
  EXPECT_LE(jackClient::impl::g_resetTimingCount, 3);
}

/**
 * Implementation specific.
 * The sampleRate() returns a plausible value.
 */
TEST_F(JackClientTest, implSampleRate) {
  auto x = jackClient::impl::sampleRate();
  EXPECT_GE(x, 22050);
  EXPECT_LE(x, 192000);
}
/**
 * Implementation specific.
 * In one second there are exactly `sampleRate` frames.
 */
TEST_F(JackClientTest, implDuration2frames) {
  using namespace std::chrono_literals;
  int sr = jackClient::impl::sampleRate();
  int x = (int)jackClient::impl::duration2frames(sysClock::SysTimeUnits(1s));
  EXPECT_EQ(x, sr);
}

/**
 * Implementation...
 * `sampleRate` frames will take one second.
 */
TEST_F(JackClientTest, implFrames2duration) {
  using namespace std::chrono_literals;
  int sr = jackClient::impl::sampleRate();
  auto x = jackClient::impl::frames2duration(sr);
  EXPECT_EQ(x, 1s);
}
/**
 * Implementation...
 * Frames2duration() should be fast.
 */
TEST_F(JackClientTest, implFrames2durationSpeed) {

  static sysClock::SysTimeUnits xx{}; // an accumulator (to cheat compiler optimization)
  constexpr int repetitions = 1000000;

  auto start = sysClock::now();
  for (int i = 0; i < repetitions; i++) {
    xx = ++jackClient::impl::frames2duration(i); // hope this will not be optimized away.
  }
  auto end = sysClock::now();

  double callsPersSecond =
      repetitions / std::chrono::duration<double, std::ratio<1, 1>>(end - start).count();
  SPDLOG_INFO("implFrames2durationSpeed - calls per second = {} c/s", callsPersSecond);

  EXPECT_GT(callsPersSecond, 10000000.0);
}

/**
 * When the jack server dies during a session, the `onServerAbend` is called.
 */
TEST_F(JackClientTest, serverAbend) {
  using namespace std::chrono_literals;

  int onServerAbendCount = 0;

  jackClient::onServerAbend([&]() { onServerAbendCount++; });

  jackClient::activate();
  std::this_thread::sleep_for(100ms);

  system("jack_control stop");
  std::this_thread::sleep_for(100ms);
  EXPECT_EQ(onServerAbendCount, 1);
}
/**
 * When the jack server shuts down after the jackClient has closed,
 * the `onServerAbend` shall not be invoked.
 */
TEST_F(JackClientTest, normalEnd) {
  using namespace std::chrono_literals;

  int onServerAbendCount = 0;

  jackClient::onServerAbend([&]() { onServerAbendCount++; });

  jackClient::activate();
  std::this_thread::sleep_for(100ms);

  jackClient::close();

  std::this_thread::sleep_for(100ms);

  system("jack_control stop");
  std::this_thread::sleep_for(100ms);

  EXPECT_EQ(onServerAbendCount, 0);
}
/**
 * Function `jackClock.now()` should be sufficiently fast.
 * This means that within a frame, we want at least 10 calls.
 * Thus one call should be shorter than 1/(44100*10) seconds
 * That is, it shall be shorter than two microseconds.
 */
TEST_F(JackClientTest, jackClockSpeed) {

  auto jackClock = jackClient::clock();
  long previousTimePoint{LONG_MIN};
  constexpr long repetitions= 1000;

  auto start = sysClock::now();
  for (int i=0;i<repetitions;i++){
    long jackNow = jackClock->now();
    // check for monotonic increase and avoid to be optimized away.
    EXPECT_GE(jackNow, previousTimePoint);
    previousTimePoint = jackNow;
  }
  auto end = sysClock::now();

  auto callDuration =  sysClock::toMicrosecondFloat(end-start)/repetitions;

  EXPECT_LT(callDuration, 2.0);
  SPDLOG_INFO("jackClockSpeed - call duration: {} us", callDuration);
}
/**
 * Function `jackClock.now()` should continue to
 * deliver monotonically increasing values even
 * when and after the JACK server is closed.
 */
TEST_F(JackClientTest, jackClockOnClose) {

  auto jackClock = jackClient::clock();
  long previousTimePoint{LONG_MIN};
  constexpr long repetitions= 1000;

  for (int i=0;i<repetitions;i++){
    long jackNow = jackClock->now();
    // check for monotonic increase and avoid to be optimized away.
    EXPECT_GE(jackNow, previousTimePoint);
    previousTimePoint = jackNow;
  }

  jackClient::close();

  for (int i=0;i<repetitions;i++){
    long jackNow = jackClock->now();
    // check for monotonic increase and avoid to be optimized away.
    EXPECT_GE(jackNow, previousTimePoint);
    previousTimePoint = jackNow;
  }
}
} // namespace unitTests
