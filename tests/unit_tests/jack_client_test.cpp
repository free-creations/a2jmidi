/*
 * File: sys_clock_test.cpp
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
#include <chrono>
#include "gtest/gtest.h"
#include <thread>

namespace unitTests {
class JackClientTest : public ::testing::Test {

protected:
  JackClientTest() {
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("JackClientTest-stared");
  }

  ~JackClientTest() override { SPDLOG_INFO("JackClientTest-ended"); }
  /**
   * Will be called right before each test.
   */
  void SetUp() override {
    EXPECT_EQ(jackClient::state(), jackClient::State::stopped);
    jackClient::open("UnitTest");
    EXPECT_EQ(jackClient::state(), jackClient::State::connected);
  }

  /**
   * Will be called immediately after each test.
   */
  void TearDown() override {
    jackClient::close();
    EXPECT_EQ(jackClient::state(), jackClient::State::stopped);
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
 * we can `activate` and `stop` the JackClient.
 */
TEST_F(JackClientTest, activateStop) {
  jackClient::activate();
  EXPECT_EQ(jackClient::state(), jackClient::State::running);

  jackClient::stop();
  EXPECT_EQ(jackClient::state(), jackClient::State::connected);
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
  EXPECT_EQ(jackClient::state(), jackClient::State::connected);
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
  int x = (int) jackClient::impl::duration2frames(sysClock::SysTimeUnits(1s));
  EXPECT_EQ(x, sr);
}

/**
 * Implementation specific.
 * `sampleRate` frames will take one second.
 */
TEST_F(JackClientTest, implFrames2duration) {
  using namespace std::chrono_literals;
  int sr = jackClient::impl::sampleRate();
  auto x = jackClient::impl::frames2duration(sr);
  EXPECT_EQ(x, 1s);
}
/**
 * Implementation specific.
 * Frames2duration() should be fast.
 */
TEST_F(JackClientTest, implFrames2durationSpeed) {

  sysClock::SysTimeUnits xx{};
  constexpr int repetitions = 1000000;

  auto start = sysClock::now();
  for (int i = 0; i < repetitions; i++) {
    xx = ++jackClient::impl::frames2duration(i);
  }
  auto end = sysClock::now();

  double callsPersSecond =
      repetitions / std::chrono::duration <double, std::ratio<1,1>> (end - start).count();
  SPDLOG_INFO("implFrames2durationSpeed - calls per second = {} c/s", callsPersSecond);

  EXPECT_GT(callsPersSecond,10000000.0);

}
} // namespace unitTests
