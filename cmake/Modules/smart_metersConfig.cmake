INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_SMART_METERS smart_meters)

FIND_PATH(
    SMART_METERS_INCLUDE_DIRS
    NAMES smart_meters/api.h
    HINTS $ENV{SMART_METERS_DIR}/include
        ${PC_SMART_METERS_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    SMART_METERS_LIBRARIES
    NAMES gnuradio-smart_meters
    HINTS $ENV{SMART_METERS_DIR}/lib
        ${PC_SMART_METERS_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/smart_metersTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SMART_METERS DEFAULT_MSG SMART_METERS_LIBRARIES SMART_METERS_INCLUDE_DIRS)
MARK_AS_ADVANCED(SMART_METERS_LIBRARIES SMART_METERS_INCLUDE_DIRS)
