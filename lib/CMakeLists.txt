#============================================================================
# File        : CMakeLists.txt
# Description : build scripts for third party libraries.
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

# SPDLOG is a very fast, C++ logging library.
# We'll build it into a compiled static library.

set(SPDLOG_MASTER_PROJECT OFF)
set(SPDLOG_BUILD_SHARED OFF)
set(SPDLOG_COMPILED_LIB 1)

# add subdirectory and use SPDLOG's own CMakeLists.txt
add_subdirectory(spdlog)