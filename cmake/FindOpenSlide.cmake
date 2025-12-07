# FindOpenSlide.cmake
# Find OpenSlide library
#
# This module defines:
#  OPENSLIDE_FOUND - whether OpenSlide was found
#  OPENSLIDE_INCLUDE_DIRS - include directories for OpenSlide
#  OPENSLIDE_LIBRARIES - libraries to link against OpenSlide

# Search in Homebrew paths (macOS) and system paths (Linux)
find_path(OPENSLIDE_INCLUDE_DIR openslide/openslide.h
    PATHS /opt/homebrew /usr/local /usr
    PATH_SUFFIXES include)

find_library(OPENSLIDE_LIBRARY
    NAMES openslide libopenslide
    PATHS /opt/homebrew /usr/local /usr
    PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSlide
    REQUIRED_VARS OPENSLIDE_LIBRARY OPENSLIDE_INCLUDE_DIR)

if(OPENSLIDE_FOUND)
    set(OPENSLIDE_LIBRARIES ${OPENSLIDE_LIBRARY})
    set(OPENSLIDE_INCLUDE_DIRS ${OPENSLIDE_INCLUDE_DIR})
    mark_as_advanced(OPENSLIDE_INCLUDE_DIR OPENSLIDE_LIBRARY)
endif()
