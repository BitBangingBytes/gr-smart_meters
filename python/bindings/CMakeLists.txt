# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT smart_meters_sources)
    MESSAGE(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT smart_meters_sources)

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

list(APPEND smart_meters_python_files
    GridStream_python.cc python_bindings.cc)

GR_PYBIND_MAKE_OOT(smart_meters 
   ../..
   gr::smart_meters
   "${smart_meters_python_files}")

install(TARGETS smart_meters_python DESTINATION ${GR_PYTHON_DIR}/smart_meters COMPONENT pythonapi)
