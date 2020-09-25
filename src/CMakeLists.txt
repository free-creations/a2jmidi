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
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DSPDLOG_ACTIVE_LEVEL=0")


# make sure the BOOST-program-options library is installed
set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS program_options)

# build the a2jmidi application executable.
add_executable(a2jmidi)
# define the sources for the application.
target_sources(a2jmidi PUBLIC
        a2jmidi_main.cpp
        a2jmidi.cpp
        a2jmidi_commandLineParser.cpp
        jack_client.cpp
        alsa_receiver_queue.cpp)
#target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ../lib)
target_link_libraries(a2jmidi  PRIVATE jack spdlog pthread asound ${Boost_LIBRARIES})

# build the a2j_remake application executable.
add_executable(a2j_remake)
# define the sources for the application.
target_sources(a2j_remake PUBLIC
        a2j_remake.cpp
        jack_client.cpp
        alsa_receiver_queue.cpp)
#target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ../lib)
target_link_libraries(a2j_remake  PRIVATE jack spdlog pthread asound ${Boost_LIBRARIES})