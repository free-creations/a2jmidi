/*
 * File: alsa_client_impl_test.cpp
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

#include "gmock/gmock.h"


namespace unitTests {
/***
 * Testing the module `AlsaClient`.
 */
class AlsaClientImplTest : public ::testing::Test {

protected:
  AlsaClientImplTest() {
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_INFO("AlsaClientImplTest-stared");
  }

  ~AlsaClientImplTest() override { SPDLOG_INFO("AlsaClientImplTest-ended"); }
};

/**
 * A port ID-string can have a colon separating the client part from the port part.
 */
TEST_F(AlsaClientImplTest, dissectPortIdentifierFindColon) {
  auto withColon = alsaClient::impl::dissectPortIdentifier("abc:def");
  EXPECT_TRUE(withColon.hasColon);

  auto withoutColon = alsaClient::impl::dissectPortIdentifier("abcdef");
  EXPECT_FALSE(withoutColon.hasColon);
}



} // namespace unitTests
