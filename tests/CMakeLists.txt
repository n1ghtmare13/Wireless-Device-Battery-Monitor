# Platform-appropriate tests. The Linux tests mirror linux/Makefile.linux;
# Windows tests remain driven by the root Makefile for now.

if(WB_BUILD_LINUX AND NOT WIN32)
    add_executable(linux_hid_test linux_hid_test.cpp)
    target_link_libraries(linux_hid_test PRIVATE wb_headers wb_drivers)
    target_compile_options(linux_hid_test PRIVATE -Wall -Wno-missing-field-initializers)
    add_test(NAME linux_hid_test COMMAND linux_hid_test)

    add_executable(json_output_test json_output_test.cpp)
    target_link_libraries(json_output_test PRIVATE wb_headers)
    add_test(NAME json_output_test COMMAND json_output_test)
endif()
