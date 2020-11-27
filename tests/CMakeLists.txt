#============================================================================
# File        : tests/CMakeLists.txt
# Description : CMake-script to activate "googletest"
#
# Copyright 2020 Harald Postner (www.free-creations.de)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#============================================================================
project(${CMAKE_PROJECT_NAME}_tests)

# note: "googletest" is setup as a git submodule for this project.
# see     https://git-scm.com/book/en/v2/Git-Tools-Submodules
set(GOOGLETEST_DIR "${CMAKE_SOURCE_DIR}/tests/lib/googletest")
if(NOT EXISTS ${GOOGLETEST_DIR})
    message(FATAL_ERROR "googletest not found.")
endif()

# go into the above downloaded dirs and execute google's CMakeLists.txt
add_subdirectory(lib/googletest)

# build the unit tests
add_subdirectory(unit_tests)