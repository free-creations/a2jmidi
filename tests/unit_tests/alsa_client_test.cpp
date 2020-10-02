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
  EXPECT_EQ(alsaClient::deviceName(), "unitTestAlsaDevice");

  alsaClient::close();

}
/**
 * we can create a port.
 */
TEST_F(AlsaClientTest, createPort) {
  alsaClient::open("unitTestAlsaDevice");

  alsaClient::newInputPort("testPort");
  EXPECT_EQ(alsaClient::portName(), "testPort");

  alsaClient::close();

}
} // namespace unitTests
