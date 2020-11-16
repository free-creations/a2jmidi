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


#include "sys_clock.h"

#include "gtest/gtest.h"

namespace unitTests {
class SysClockTest : public ::testing::Test {};

/**
 * We rely on the fact that the system clock ticks in nanoseconds.
 * If clock resolution is less than microseconds it still might work,
 * but timing might be imprecise.
 */
TEST_F(SysClockTest, timingResolutionOK) {

  EXPECT_GE(sysClock::TICKS_PER_SECOND, 1000000000);
}

/**
 * The sysClock can be read using a construct like `sysClock::now()`.
 */
TEST_F(SysClockTest, readClock) {

  sysClock::TimePoint timePoint1 = sysClock::now();
  sysClock::TimePoint timePoint2 = sysClock::now();
  EXPECT_GT(timePoint2, timePoint1);
}
/**
 * The difference of two `sysClock::TimePoint`s is a `sysClock::SysTimeUnits`.
 */
TEST_F(SysClockTest, useDurations) {

  sysClock::TimePoint timePoint1 = sysClock::now();
  sysClock::TimePoint timePoint2 = sysClock::now();

  // the difference of two time points gives a duration (measured in system-time-units).
  sysClock::SysTimeUnits duration = timePoint2 - timePoint1;

  EXPECT_GT(duration.count(), 0);
}


/**
 * floating point microsecond can be transformed to microseconds.
 */
TEST_F(SysClockTest, fromFloatUsToSystemUnits) {

  auto x = sysClock::toSysTimeUnits(0.5559);
  // we rely on the fact that the system clock ticks in nanoseconds.
  // So, 0.5559 microseconds are 555.9 nanoseconds.
  EXPECT_EQ(x.count(),555);
}
/**
 * Durations given in SysTimeUnits can be transformed to microseconds expressed as a floating point
 * value.
 */
TEST_F(SysClockTest, toMicrosecondFloat) {
  using namespace std::chrono_literals;

  auto x = sysClock::SysTimeUnits{55us}; // duration of 55 microseconds
  EXPECT_EQ(sysClock::toMicrosecondFloat(x),55.0);

  auto y = sysClock::SysTimeUnits{55ns}; // duration of 55 nanoseconds
  EXPECT_EQ(sysClock::toMicrosecondFloat(y),0.055);
}

} // namespace unitTests
