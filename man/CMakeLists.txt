#============================================================================
# File        : man/CMakeLists.txt
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


# Install the unix manual pages.
#
# Note: to convert `a2jmidi.adoc` into `a2jmidi.1`
# (conversion of *asciiDoc* into  Unix default *troff* format) use the following:
#       $ asciidoctor -b manpage a2jmidi.adoc
include(GNUInstallDirs)
install(FILES a2jmidi.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)