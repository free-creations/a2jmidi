# FindJACK
# --------
#
# Find jack
#
# Find the jack (-audio) library and headers
#
# ::
#
#
#   This module defines the following variables:
#
# JACK_FOUND          -  set to 1 if module(s) exist
# JACK_LIBRARIES      -  the jack library
# JACK_LIBRARY        -  same as JACK_LIBRARIES
# JACK_LDFLAGS        -  all required linker flags
# JACK_VERSION        -  version of the module
# JACK_INCLUDE_DIRS   -  where to find jack/jack.h, etc.
# JACK_INCLUDE_DIR    -  same as JACK_INCLUDE_DIRS
# JACK_LIBRARY_DIR    -  where to find the jack library
# JACK_LIBRARY_DIRS   -  same as JACK_LIBRARY_DIR

#============================================================================
# File        : FindJACK.cmake
# Description : CMake-script to find the jack (-audio) library and headers
#
# Copyright 2019 Harald Postner (www.free-creations.de)
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

# We'll use `pkg-config` to query for an installed jack library.
find_package(PkgConfig QUIET REQUIRED)
#

# the call to pkg_check_modules will set the following variables
# JACK_FOUND          ... set to 1 if module(s) exist
# JACK_LIBRARIES      ... only the libraries (w/o the '-l')
# JACK_LDFLAGS        ... all required linker flags
# JACK_VERSION        ... version of the module
pkg_check_modules(JACK QUIET jack)

if (JACK_FOUND)

    # there is only one library for JACK
    set(JACK_LIBRARY ${JACK_LIBRARIES})

    # The "include dir" is defined as an extra variable in pkg-configs .pc file
    # (on my system it is `/usr/lib/x86_64-linux-gnu/pkgconfig/jack.pc`)
    pkg_get_variable(JACK_INCLUDE_DIR jack includedir)
    # we'll set the plural to th same value
    set(JACK_INCLUDE_DIRS ${JACK_INCLUDE_DIR})

    # same for libdir
    pkg_get_variable(JACK_LIBRARY_DIR jack libdir)
    # and just as above
    set(JACK_LIBRARY_DIRS ${JACK_LIBRARY_DIR})


    # we'll also retrieve the "server_libs" variable from pkg-config
    pkg_get_variable(JACK_SERVER_LIBS jack server_libs)

endif ()

# the following function will take care of the REQUIRED, QUIET and version-related arguments
find_package_handle_standard_args(JACK
        REQUIRED_VARS JACK_LIBRARY JACK_INCLUDE_DIR
        VERSION_VAR JACK_VERSION
        FAIL_MESSAGE "Could NOT find the JACK Audio Connection Kit development files. Please make sure that this package is installed.")

# mark_as_advanced(JACK_INCLUDE_DIR JACK_LIBRARY)