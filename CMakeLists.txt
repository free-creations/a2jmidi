#============================================================================
# File        : CMakeLists.txt
# Description : CMake-script to build and test the a_j_midi executable.
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
cmake_minimum_required(VERSION 3.16)

project(a_j_midi)

# the version shall be given on the command line, otherwise we'll use "x.x.x"
set(VERSION "x.x.x" CACHE STRING "The project version")


# Enable the custom-module "FindJACK.cmake" in the cmake/Modules subdirectory.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# We need the "JACK-development" package to be installed on the  build platform
# in order to have the "jack.h" headers and the jack library to link to.
find_package(JACK REQUIRED)

# Let us use C++ 17 standard.
set(CMAKE_CXX_STANDARD 17)

# In the "Debug" build type we'll set a preprocessor variable "DEBUG"
add_compile_definitions("$<$<CONFIG:DEBUG>:DEBUG=1>")

# All project sources reside here.
add_subdirectory(src)

# All third party code shall be placed in the "lib" subdirectory
add_subdirectory(lib)

# Build the unit-tests only in debug mode
IF(${CMAKE_BUILD_TYPE} STREQUAL Debug)
   add_subdirectory(tests)
ENDIF()
