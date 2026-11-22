cmake_minimum_required(VERSION 3.16)

project(WirelessDeviceBatteryMonitor
    VERSION 1.0.0
    DESCRIPTION "Battery monitor for wireless devices"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(WB_BUILD_LINUX "Build the Linux GTK4 port" ON)
option(WB_BUILD_TESTS "Build the test suite" ON)
option(WB_BUILD_WINDOWS "Build the Windows MinGW port" OFF)

include(WBVersion)

if(WB_BUILD_LINUX AND NOT WIN32)
    add_subdirectory(src)
    add_subdirectory(src/linux)
    include(WBInstall)
endif()

if(WB_BUILD_WINDOWS)
    add_subdirectory(src/windows)
endif()

if(WB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(WB_BUILD_LINUX AND NOT WIN32)
    include(WBPackaging)
endif()
