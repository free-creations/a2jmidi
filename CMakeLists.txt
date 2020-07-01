#============================================================================
# File        : CMakeLists.txt
# Description : CMake-script to build and test the a_j_midi executable.
#
# Copyright 2019 Harald Postner (Harald at free-creations.de)
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
cmake_minimum_required(VERSION 3.1)

project(a_j_midi)

# the version shall be given on the command line (by build.gradle), otherwise we'll use "x.x.x"
set(VERSION "x.x.x" CACHE STRING "The project version")

# The directory where the final artifacts should be delivered shall be given on given on the command line,
#   otherwise we'll use "build/out".
set(DELIVERIES_DIR "${PROJECT_BINARY_DIR}/out" CACHE STRING "The directory where the executable should be delivered")

# The unit tests take quite long to compile. So we make building with unit-tests an option.
set(WITH_UNIT_TESTS ON CACHE BOOL "Should unit-test be build.")


# Enable the custom-module "FindJACK.cmake" in the cmake/Modules subdirectory.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# We need the "JACK-development" package to be installed on the  build platform
# in order to have the "jack.h" headers and the jack library to link to.
find_package(JACK REQUIRED)

# Let us use C++ 17 standard.
set(CMAKE_CXX_STANDARD 17)

# Write the generated library into the DELIVERIES_DIR selected above.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${DELIVERIES_DIR})

# All project sources reside here.
add_subdirectory(src)

# Build the unit-tests only if explicitly requested
IF(${WITH_UNIT_TESTS})
   add_subdirectory(tests)
ENDIF(${WITH_UNIT_TESTS})
