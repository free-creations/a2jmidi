/*
 * File: alsa_util_test.cpp
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

#include "alsa_util.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"


namespace unitTests {
/***
 * Testing the module `AlsaClient`.
 */
class AlsaUtilTest : public ::testing::Test {

protected:
  AlsaUtilTest() {
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("AlsaUtilTest-stared");
  }

  ~AlsaUtilTest() override { SPDLOG_INFO("AlsaUtilTest-ended"); }
};




/**
 *
 */
TEST_F(AlsaUtilTest, reportError) {
  bool result = ALSA_ERROR(-23, "open file");
  EXPECT_TRUE(result);
}

TEST_F(AlsaUtilTest, reportNothing) {
  bool result = ALSA_ERROR(0, "open file");
  EXPECT_FALSE(result);
}


} // namespace unitTests
