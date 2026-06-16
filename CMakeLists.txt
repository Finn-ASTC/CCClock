cmake_minimum_required(VERSION 3.20)
project(Clock C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT MINGW)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

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
    src/clk_clock.c
    src/clk_menu.c
    src/clk_menu_theme.c
)

target_include_directories(libclock PRIVATE include)

add_executable(Clock src/main.c)

target_link_libraries(Clock PRIVATE libclock)
target_include_directories(Clock PRIVATE include)

add_executable(clock_demo sandbox/clock_demo.c)

target_link_libraries(clock_demo PRIVATE libclock)
target_include_directories(clock_demo PRIVATE include)

add_executable(json_benchmark sandbox/json_benchmark.c)

target_link_libraries(json_benchmark PRIVATE libclock)
target_include_directories(json_benchmark PRIVATE include)

enable_testing()

add_clk_test(key_io)
add_clk_test(json)
add_clk_test(term)
add_clk_test(clock)
add_clk_test(menu)
add_clk_test(menu_theme)
