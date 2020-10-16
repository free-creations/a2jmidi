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
#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-lambda-function-name"

#include "alsa_client.h"
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"

#include "gmock/gmock.h"

namespace unitTests {
/***
 * Testing the implementation of module `AlsaClient`.
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
TEST_F(AlsaClientImplTest, toProfileNoColon) {
  auto withoutColon = alsaClient::impl::toProfile("abcdef");
  EXPECT_FALSE(withoutColon.hasColon);
  EXPECT_EQ(withoutColon.firstName, "abcdef");
  EXPECT_TRUE(withoutColon.secondName.empty());
  EXPECT_EQ(withoutColon.firstInt, alsaClient::NULL_ID);
  EXPECT_EQ(withoutColon.secondInt, alsaClient::NULL_ID);
}

/**
 * A port ID-string can have a colon separating the client part from the port part.
 */
TEST_F(AlsaClientImplTest, toProfileHasColon) {
  auto withColon = alsaClient::impl::toProfile("abc:def");
  EXPECT_TRUE(withColon.hasColon);
  EXPECT_EQ(withColon.firstName, "abc");
  EXPECT_EQ(withColon.secondName, "def");
  EXPECT_EQ(withColon.firstInt, alsaClient::NULL_ID);
  EXPECT_EQ(withColon.secondInt, alsaClient::NULL_ID);
}
/**
 * A port ID-string can be specified as two numbers separated by colon
 */
TEST_F(AlsaClientImplTest, toProfileNumeric) {
  auto withColon = alsaClient::impl::toProfile("128:01");
  EXPECT_TRUE(withColon.hasColon);
  EXPECT_EQ(withColon.firstName, "128");
  EXPECT_EQ(withColon.secondName, "01");
  EXPECT_EQ(withColon.firstInt, 128);
  EXPECT_EQ(withColon.secondInt, 1);
}

/**
 * The port identifier shall not be empty
 */
TEST_F(AlsaClientImplTest, toProfileErrorEmptyString) {
  auto badIdentifier = alsaClient::impl::toProfile("");
  EXPECT_TRUE(badIdentifier.hasError);
  SPDLOG_TRACE("Message: {}", badIdentifier.errorMessage.str());
}

/**
 * The port identifier shall not be empty
 */
TEST_F(AlsaClientImplTest, toProfileErrorEmptyParts) {
  auto badIdentifier = alsaClient::impl::toProfile(":");
  EXPECT_TRUE(badIdentifier.hasError);
  SPDLOG_TRACE("Message: {}", badIdentifier.errorMessage.str());
}
/**
 * The port identifier shall not have more than one colon
 */
TEST_F(AlsaClientImplTest, toProfileErrorTwoColons) {
  auto badIdentifier = alsaClient::impl::toProfile("a:b:c");
  EXPECT_TRUE(badIdentifier.hasError);
  SPDLOG_TRACE("Message: {}", badIdentifier.errorMessage.str());
}
/**
 * first part shall not be empty
 */
TEST_F(AlsaClientImplTest, toProfileErrorMissingFirst) {
  auto badIdentifier = alsaClient::impl::toProfile(":c");
  EXPECT_TRUE(badIdentifier.hasError);
  SPDLOG_TRACE("Message: {}", badIdentifier.errorMessage.str());
}
/**
 * second part shall not be empty
 */
TEST_F(AlsaClientImplTest, toProfileErrorMissingSecond) {
  auto badIdentifier = alsaClient::impl::toProfile("a:");
  EXPECT_TRUE(badIdentifier.hasError);
  SPDLOG_TRACE("Message: {}", badIdentifier.errorMessage.str());
}

/**
 * Function `identifierStrToInt` shall return the given identifier as an integral number.
 */
TEST_F(AlsaClientImplTest, identifierStrToInt) {
  auto number = alsaClient::impl::identifierStrToInt(" 4711 ");
  EXPECT_EQ(number, 4711);
}
/**
 * If the the given identifier cannot be converted to an integral number, it shall return NULL_ID.
 */
TEST_F(AlsaClientImplTest, identifierStrToNullInt) {
  auto number = alsaClient::impl::identifierStrToInt(" abc ");
  EXPECT_EQ(number, alsaClient::NULL_ID);
}

/**
 * a normalized identifier has all blank-characters removed.
 */
TEST_F(AlsaClientImplTest, normalizedIdentifierNoBlanks) {
  auto normal = alsaClient::impl::normalizedIdentifier(" abc d   e f");
  EXPECT_EQ(normal, "abcdef");
}
/**
 * a _normalized identifier_ has all special characters replaced by underscore.
 */
TEST_F(AlsaClientImplTest, normalizedIdentifierNoSpecials) {
  auto normal = alsaClient::impl::normalizedIdentifier("a!\"§$%&/()=?{[]}*+~#;,:.-x");
  EXPECT_EQ(normal, "a_________________________x");
  // due to problems with Unicode, multibyte characters might be translated into two underscores.
  auto umlaute = alsaClient::impl::normalizedIdentifier("äxÄxöxÖxüxÜx");
  EXPECT_EQ(umlaute, "__x__x__x__x__x__x");
}

/**
 * `findPort` shall check all ports until a match is found.
 */
TEST_F(AlsaClientImplTest, findPort) {
  using namespace ::alsaClient::impl;
  alsaClient::open("findPort");

  PortProfile requested;

  int visitedPortsCount = 0;
  auto result = findPort( //
      requested,
      [&visitedPortsCount](PortCaps caps, PortID port, const std::string &clientName,
                           const std::string &portName, const PortProfile &search) -> bool {
        SPDLOG_TRACE("matching - [{}:{}]  [{}:{}]", port.client, port.port, clientName, portName);
        visitedPortsCount++;
        return false; //< never match! => We'll traverse every port.
      });

  // there is no result because we never match.
  EXPECT_EQ(result, NULL_PORT_ID);

  // every installation has at least three predefined ports these are:
  // [System:Timer], [System:Announce] and [Midi Through:Midi Through Port-0]
  EXPECT_GE(visitedPortsCount,
            3); //< at least [System:Timer], [System:Announce], [Midi Through:Midi Through Port-0]

  alsaClient::close();
}
/**
 * match shall be true when the requested PortId matches the actual PortId
 */
TEST_F(AlsaClientImplTest, matchExactPortID) {
  using namespace ::alsaClient::impl;
  PortID actualPort{1, 2};
  PortCaps actualCaps{SENDER_PORT};

  PortProfile requestedProfile;
  requestedProfile.caps = SENDER_PORT;
  requestedProfile.firstInt = actualPort.client;
  requestedProfile.secondInt = actualPort.port;

  bool result = match(actualCaps, actualPort, "TestDevice", "sender", requestedProfile);
  EXPECT_TRUE(result);
}
/**
 * match shall only be true when the actual Port Capabilities a least fulfill all requested
 * Capabilities.
 */
TEST_F(AlsaClientImplTest, fulfillsCaps) {
  using namespace ::alsaClient::impl;
  // return true when actualCaps equal the requestedCaps
  EXPECT_TRUE(fulfills(SENDER_PORT, SENDER_PORT));
  // return true when actualCaps over-fulfill the requestedCaps
  EXPECT_TRUE(fulfills(SENDER_PORT | RECEIVER_PORT, SENDER_PORT));
  // return false when actualCaps only partially fulfill the requestedCaps.
  EXPECT_FALSE(fulfills(SND_SEQ_PORT_CAP_READ, SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ));
}
} // namespace unitTests

#pragma clang diagnostic pop