# Copyright 2011,2012 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

list(APPEND lora_sources
    decoder_impl.cc
    message_file_sink_impl.cc
    message_socket_sink_impl.cc
    channelizer_impl.cc
    controller_impl.cc
    debugger.cc
    message_socket_source_impl.cc
)

set(lora_sources "${lora_sources}" PARENT_SCOPE)
if(NOT lora_sources)
    MESSAGE(STATUS "No C++ sources... skipping lib/")
    return()
endif(NOT lora_sources)

add_library(gnuradio-lora SHARED ${lora_sources})
target_link_libraries(gnuradio-lora ${Boost_LIBRARIES} ${GNURADIO_ALL_LIBRARIES} liquid)
set_target_properties(gnuradio-lora PROPERTIES DEFINE_SYMBOL "gnuradio_lora_EXPORTS")

if(APPLE)
    set_target_properties(gnuradio-lora PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
    )
endif(APPLE)

########################################################################
# Install built library files
########################################################################
install(TARGETS gnuradio-lora
    LIBRARY DESTINATION lib${LIB_SUFFIX} # .so/.dylib file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
    RUNTIME DESTINATION bin              # .dll file
)

########################################################################
# Build and register unit test
########################################################################
include(GrTest)

include_directories(${CPPUNIT_INCLUDE_DIRS})

list(APPEND test_lora_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/test_lora.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_lora.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_message_socket_sink.cc
)

add_executable(test-lora ${test_lora_sources})

target_link_libraries(
  test-lora
  ${GNURADIO_RUNTIME_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CPPUNIT_LIBRARIES}
  gnuradio-lora
  liquid
)

GR_ADD_TEST(test_lora test-lora)
