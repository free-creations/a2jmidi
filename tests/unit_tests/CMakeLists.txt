#============================================================================
# File        : CMakeLists.txt
# Description : CMake-script to build and run the unittests.
#
# Copyright 2020 Harald Postner (www.free-creations.de)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#============================================================================

include_directories("${gtest_SOURCE_DIR}/include" ${gtest_SOURCE_DIR})

set(UNIT_TEST_EXE_NAME unit_tests_run)

# we'll need the "BOOST-program-options library" also here for testing a2jmidi_commandLineParser_test
set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(${UNIT_TEST_EXE_NAME} ${target_sources})
target_sources(${UNIT_TEST_EXE_NAME} PUBLIC
        # list all source files that shall be tested
        "${CMAKE_SOURCE_DIR}/src/alsa_receiver_queue.cpp"
        "${CMAKE_SOURCE_DIR}/src/alsa_client.cpp"
        "${CMAKE_SOURCE_DIR}/src/jack_client.cpp"
        "${CMAKE_SOURCE_DIR}/src/a2jmidi_commandLineParser.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"

        # list all files that do, or help to do, the tests.
        alsa_helper.cpp
        alsa_helper_test.cpp
        alsa_client_test.cpp
        alsa_client_impl_test.cpp
        alsa_util_test.cpp
        alsa_receiver_queue_test.cpp
        sys_clock_test.cpp
        jack_client_test.cpp
        jack_client_test_no_server.cpp
        a2jmidi_commandLineParser_test.cpp)

target_link_libraries(${UNIT_TEST_EXE_NAME} spdlog pthread jack asound gtest gtest_main gmock gmock_main ${Boost_LIBRARIES})
target_include_directories(${UNIT_TEST_EXE_NAME} PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/tests/lib")


# A custom command that produces version.cpp, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/version.cpp" "${CMAKE_CURRENT_BINARY_DIR}/_version.cpp"
        COMMAND ${CMAKE_COMMAND} -P  "${CMAKE_MODULE_PATH}/Version.cmake"
        COMMENT "Generating new Version file")

