# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT lora_sources)
    MESSAGE(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT lora_sources)

########################################################################
# Check for pygccxml
########################################################################
GR_PYTHON_CHECK_MODULE_RAW(
    "pygccxml"
    "import pygccxml"
    PYGCCXML_FOUND
    )

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND lora_python_files
    channelizer_python.cc
    controller_python.cc
    debugger_python.cc
    decoder_python.cc
    message_file_sink_python.cc
    message_socket_sink_python.cc
    message_socket_source_python.cc python_bindings.cc)

GR_PYBIND_MAKE_OOT(lora
   ../..
   gr::lora
   "${lora_python_files}")

install(TARGETS lora_python DESTINATION ${GR_PYTHON_DIR}/lora COMPONENT pythonapi)
