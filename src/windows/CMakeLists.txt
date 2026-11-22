# Windows port: MinGW-w64 build of BatteryHub.exe.
# Mirrors the root Makefile. Build from an MSYS2 MINGW64/UCRT64 shell, e.g.
#   cmake -S . -B build-cmake -DWB_BUILD_WINDOWS=ON -DWB_BUILD_LINUX=OFF

find_program(WINDRES NAMES windres REQUIRED)

set(WB_BUILD_DIR "${PROJECT_SOURCE_DIR}/build")

# languages.txt: every lang/*.txt joined with an "@file <name>" marker line,
# exactly as the root Makefile's build/languages.txt rule does.
set(WB_LANGUAGES_TXT "${WB_BUILD_DIR}/languages.txt")
file(GLOB _wb_lang_files "${PROJECT_SOURCE_DIR}/lang/*.txt")
list(SORT _wb_lang_files)
set(_wb_languages "")
foreach(_lang IN LISTS _wb_lang_files)
    get_filename_component(_lang_name "${_lang}" NAME_WE)
    string(APPEND _wb_languages "@file ${_lang_name}\n")
    file(READ "${_lang}" _lang_body)
    string(APPEND _wb_languages "${_lang_body}\n")
endforeach()
file(WRITE "${WB_LANGUAGES_TXT}" "${_wb_languages}")

# app.rc uses "../build/version.h" relative to res/. Keep that include intact
# and place the adjusted copy in res/ so both version.h and languages.txt
# resolve from the source-tree build directory.
file(READ "${PROJECT_SOURCE_DIR}/res/app.rc" _wb_rc)
set(WB_APP_RC "${WB_BUILD_DIR}/app.rc")
file(WRITE "${WB_APP_RC}" "${_wb_rc}")

set(WB_APP_RES "${WB_BUILD_DIR}/app_res.o")
add_custom_command(
    OUTPUT "${WB_APP_RES}"
    COMMAND "${WINDRES}" -i "${PROJECT_SOURCE_DIR}/res/app.rc" -o "${WB_APP_RES}"
            --include-dir "${PROJECT_SOURCE_DIR}/res"
            --include-dir "${WB_BUILD_DIR}"
    DEPENDS "${WB_LANGUAGES_TXT}" "${WB_VERSION_HEADER}"
    COMMENT "Compiling Windows application resources")
set_source_files_properties("${WB_APP_RES}" PROPERTIES GENERATED TRUE)

file(GLOB WB_WIN_SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB WB_WIN_DRIVERS "${PROJECT_SOURCE_DIR}/src/drivers/*.cpp")

add_executable(BatteryHub WIN32
    ${WB_WIN_SOURCES}
    ${WB_WIN_DRIVERS}
    "${WB_APP_RES}")
set_target_properties(BatteryHub PROPERTIES OUTPUT_NAME "BatteryHub")
target_include_directories(BatteryHub PRIVATE "${WB_BUILD_DIR}")
target_compile_options(BatteryHub PRIVATE
    -municode -Os -Wall -Wno-missing-field-initializers
    -ffunction-sections -fdata-sections
    -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00)
target_link_options(BatteryHub PRIVATE
    -municode -mwindows -static -s -Wl,--gc-sections)
target_link_libraries(BatteryHub PRIVATE
    hid setupapi cfgmgr32 ole32 oleaut32 comctl32 gdi32
    runtimeobject ws2_32 winhttp bcrypt version bluetoothapis d3d11 dxgi d2d1 dcomp dwmapi)
