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


#include "sys_clock.h"

#include "gtest/gtest.h"

namespace unitTests {
class SysClockTest : public ::testing::Test {};

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
 * _system-time-units_ can be transformed into Microseconds using the function
 * `sysClock::toMicroseconds()`.
 */
TEST_F(SysClockTest, convertToMicroseconds) {

  sysClock::TimePoint timePoint1 = sysClock::now();
  sysClock::TimePoint timePoint2 = timePoint1 + sysClock::Microseconds(4711);
  // a duration expressed  in system-time-units.
  sysClock::SysTimeUnits durationSysTime = timePoint2 - timePoint1;

  // convert from _system-time-units_ to microseconds.
  sysClock::Microseconds durationUs = sysClock::toMicroseconds(durationSysTime);

  EXPECT_EQ(durationUs.count(), 4711);
}

/**
 * _system-time-units_ can be transformed into an integer using the function
 * `sysClock::toMicrosecondCount()`.
 */
TEST_F(SysClockTest, convertToMicrosecondCount) {

  sysClock::TimePoint timePoint1 = sysClock::now();
  sysClock::TimePoint timePoint2 = timePoint1 + sysClock::Microseconds(4711);
  // a duration expressed  in system-time-units.
  sysClock::SysTimeUnits durationSysTime = timePoint2 - timePoint1;

  // convert from _system-time-units_ to microseconds.
  long durationUs = sysClock::toMicrosecondCount(durationSysTime);

  EXPECT_EQ(durationUs, 4711);
}
} // namespace unitTests
