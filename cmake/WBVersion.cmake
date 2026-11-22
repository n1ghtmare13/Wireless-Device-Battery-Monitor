# Generate the version header for both ports, mirroring scripts/generate-version.ps1.
# The Windows C++ sources include "../build/version.h" (relative to src/), so the
# header must exist at <source>/build/version.h; the Linux build consumes it via
# the wb_headers interface target. Override with -DWB_VERSION=MAJOR.MINOR.PATCH
# -DWB_CHANNEL=dev|test|stable.

set(WB_VERSION "${PROJECT_VERSION}" CACHE STRING "Application version (MAJOR.MINOR.PATCH)")
set(WB_CHANNEL "stable" CACHE STRING "Build channel (dev, test or stable)")

if(NOT WB_CHANNEL MATCHES "^(dev|test|stable)$")
    message(FATAL_ERROR "WB_CHANNEL must be dev, test or stable, got '${WB_CHANNEL}'")
endif()
if(NOT WB_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
    message(FATAL_ERROR "WB_VERSION must be MAJOR.MINOR.PATCH without leading zeros, got '${WB_VERSION}'")
endif()

string(REPLACE "." ";" _wb_version_parts "${WB_VERSION}")
list(GET _wb_version_parts 0 WB_VERSION_MAJOR)
list(GET _wb_version_parts 1 WB_VERSION_MINOR)
list(GET _wb_version_parts 2 WB_VERSION_PATCH)

foreach(_part IN ITEMS WB_VERSION_MAJOR WB_VERSION_MINOR WB_VERSION_PATCH)
    if(${_part} GREATER 65535)
        message(FATAL_ERROR "Version components must be <= 65535, got '${WB_VERSION}'")
    endif()
endforeach()

if(WB_CHANNEL STREQUAL "stable" AND WB_VERSION STREQUAL "0.0.0")
    message(FATAL_ERROR "A stable channel cannot use version 0.0.0")
endif()

if(WB_CHANNEL STREQUAL "stable")
    set(WB_OFFICIAL_RELEASE 1)
else()
    set(WB_OFFICIAL_RELEASE 0)
endif()

# Match generate-version.ps1: emit into <source>/build so the literal
# "../build/version.h" include in src/*.cpp resolves for the Windows port.
set(WB_VERSION_DIR "${PROJECT_SOURCE_DIR}/build")
file(MAKE_DIRECTORY "${WB_VERSION_DIR}")
set(WB_VERSION_HEADER "${WB_VERSION_DIR}/version.h")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/version.h.in"
    "${WB_VERSION_HEADER}"
    @ONLY)
