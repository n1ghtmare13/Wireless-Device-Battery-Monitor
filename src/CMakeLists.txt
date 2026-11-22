# Shared, header-only helpers and the portable driver sources live here.
# Per-port translation units live in src/linux and src/windows.

add_library(wb_headers INTERFACE)
target_include_directories(wb_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/build")

add_subdirectory(drivers)
