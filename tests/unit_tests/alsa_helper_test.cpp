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



#include "alsa_helper.h"
#include <chrono>
#include "gtest/gtest.h"
#include "spdlog/spdlog.h"
#include <thread>


// The fixture for testing class AlsaHelper.
class AlsaHelperTest : public ::testing::Test {

protected:
  unit_test_helpers::AlsaHelper alsaHelper;
    // You can remove any or all of the following functions if their bodies would
    // be empty.

    AlsaHelperTest(): alsaHelper{}{
      spdlog::set_level(spdlog::level::trace); // Set global log level
    }


    ~AlsaHelperTest()  override =  default;

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    void SetUp() override {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    void TearDown() override {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Class members declared here can be used by all tests in the test suite
    // for Clock.
};


TEST_F(AlsaHelperTest, openAlsaSequencer) {
  alsaHelper.openAlsaSequencer();
  auto hEmitterPort = alsaHelper.createOutputPort("out");
  auto hReceiverPort = alsaHelper.createInputPort("in");

  alsaHelper.connectPorts(hEmitterPort, hReceiverPort);

  alsaHelper.sendEvents(hEmitterPort,10,100);


}


