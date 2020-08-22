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

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(UNIT_TEST_EXE_NAME unit_tests_run)

add_executable(${UNIT_TEST_EXE_NAME} ${target_sources})
target_sources(${UNIT_TEST_EXE_NAME} PUBLIC
        # list all source files that shall be tested
        ${CMAKE_SOURCE_DIR}/src/clock.cpp
        ${CMAKE_SOURCE_DIR}/src/alsa_receiver_queue.cpp

        # list all files that do or help to do tests.
        alsa_helper.cpp
        alsa_helper_test.cpp
        alsa_receiver_queue_test.cpp
        clock_test.cpp)

target_link_libraries(${UNIT_TEST_EXE_NAME} spdlog pthread asound gtest gtest_main)
target_include_directories(${UNIT_TEST_EXE_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/lib)




