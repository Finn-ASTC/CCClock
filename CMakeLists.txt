cmake_minimum_required(VERSION 3.25)

# ---- vcpkg auto-detection ----
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Vcpkg auto-detected: ${CMAKE_TOOLCHAIN_FILE}")
elseif(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(WARNING "VCPKG_ROOT not set — pass -DCMAKE_TOOLCHAIN_FILE=... or set VCPKG_ROOT")
endif()

project(Clock
    VERSION 0.1.0
    DESCRIPTION "Terminal clock with ASCII art rendering, timer and alarm"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- expose compile_commands.json at project root for clangd ----
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    file(CREATE_LINK
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
        SYMBOLIC COPY_ON_ERROR)
endif()

# ---- sanitizers (Linux only, skip MinGW) ----
if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT WIN32 AND NOT MINGW)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---- static library ----
function(add_clk_test name)
    add_executable(test_${name} test/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE libclock)
    target_include_directories(test_${name} PRIVATE include)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

add_library(libclock STATIC
    src/clk_json.c
    src/clk_key_io.c
    src/clk_term.c
    src/clk_file_util.c
    src/clk_fs_watch.c
    src/clk_time.c
    src/clk_clock.c
    src/clk_clock_menu.c
    src/clk_ascii_render.c
    src/clk_menu.c
    src/clk_menu_theme.c
    src/clk_menu_instance.c
    src/clk_audio.c
)
target_include_directories(libclock PRIVATE include)

find_path(MINIAUDIO_INCLUDE_DIR
    NAMES miniaudio.h
    REQUIRED
)
target_include_directories(libclock PRIVATE "${MINIAUDIO_INCLUDE_DIR}")

if(WIN32)
    target_link_libraries(libclock PRIVATE winmm ole32)
else()
    target_link_libraries(libclock PRIVATE pthread dl m)
endif()

# ---- executables ----
macro(add_clk_exec name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE libclock)
    target_include_directories(${name} PRIVATE include)
endmacro()

add_clk_exec(Clock src/main.c)
add_clk_exec(clock_demo sandbox/clock_demo.c)
add_clk_exec(audio_demo sandbox/audio_demo.c)
add_clk_exec(json_benchmark sandbox/json_benchmark.c)
add_clk_exec(menu_demo sandbox/menu_demo.c)

enable_testing()
add_clk_test(key_io)
add_clk_test(json)
add_clk_test(term)
add_clk_test(clock)
add_clk_test(clock_sched)
add_clk_test(clock_menu)
add_clk_test(fs)
add_clk_test(menu)
add_clk_test(menu_theme)
add_clk_test(menu_instance)
