#============================================================================
# File        : CMakeLists.txt
# Description : 
#
# Copyright 2020 Harald Postner (Harald at free-creations.de)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#============================================================================


# set the SPDLOG_ACTIVE_LEVEL
# This level controls which logging messages shall be compiled
# and which messages can be discarded at compile time.
# Setting SPDLOG_ACTIVE_LEVEL to 0 means, all messages (including trace messages)
# shall be compiled.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSPDLOG_ACTIVE_LEVEL=0")



# build the a2jmidi application executable.
add_executable(a2jmidi)
# define the sources for the application.
target_sources(a2jmidi PUBLIC
        a2jmidi.cpp
        a2jmidi_commandLineParser.cpp
        a2jmidi_main.cpp
        alsa_client.cpp
        alsa_receiver_queue.cpp
        jack_client.cpp
        version.cpp)
#target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ../lib)
target_link_libraries(a2jmidi PRIVATE jack spdlog pthread asound ${Boost_LIBRARIES})


# A custom command that produces version.cpp, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build.
# Based on an idea of Matt Keeter.
# Published here: https://www.mattkeeter.com/blog/2018-01-06-versioning/
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/version.cpp" "${CMAKE_CURRENT_BINARY_DIR}/_version.cpp"
        COMMAND ${CMAKE_COMMAND} -P  "${CMAKE_MODULE_PATH}/Version.cmake"
        COMMENT "Generating new Version file")