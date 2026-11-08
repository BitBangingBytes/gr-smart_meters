#
# Copyright 2008,2009 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# The presence of this file turns this directory into a Python package

'''
This is the GNU Radio SMART_METERS module. Place your Python package
description here (python/__init__.py).
'''
import os

# import pybind11 generated symbols into the smart_meters namespace
try:
    # this might fail if the module is python-only
    from .smart_meters_python import *
except ModuleNotFoundError:
    pass

# import any pure python here.
# google_map / google_earth are optional visualization blocks that pull third-party
# deps (gmplot / simplekml) at import time. Guard them like the pybind import above so
# the core decode module (Deframer, GridStream, GridStreamDecrypt) imports with no map
# deps installed -- gmplot/simplekml are only needed if you actually use these blocks.
try:
    from .google_map import google_map
except ModuleNotFoundError:
    pass
try:
    from .google_earth import google_earth
except ModuleNotFoundError:
    pass
#
