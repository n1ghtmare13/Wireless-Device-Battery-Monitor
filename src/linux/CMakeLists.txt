# Linux port: hidraw backend + GTK4/GIO StatusNotifierItem tray application.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4 cairo)

add_library(wb_linux_backend STATIC
    hid_linux.cpp
    devices_linux.cpp)
target_link_libraries(wb_linux_backend PUBLIC wb_headers wb_drivers)
target_compile_options(wb_linux_backend PRIVATE -Wall -Wno-missing-field-initializers)

add_executable(wireless-battery main_linux.cpp)
target_link_libraries(wireless-battery PRIVATE
    wb_linux_backend
    wb_drivers
    wb_headers
    PkgConfig::GTK4)
target_link_options(wireless-battery PRIVATE -pthread)
