cmake_minimum_required(VERSION 3.20)
project(Clock C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(libclock STATIC
    src/clk_json.c
    src/clk_key_io.c
    src/clk_term.c
    src/clk_clock.c
    src/clk_menu.c
)

target_include_directories(libclock PRIVATE include)

add_executable(Clock src/main.c)

target_link_libraries(Clock PRIVATE libclock)
target_include_directories(Clock PRIVATE include)

add_executable(sandbox sandbox/main.c)

target_link_libraries(sandbox PRIVATE libclock)
target_include_directories(sandbox PRIVATE include)

enable_testing()

add_executable(test_key_io test/test_key_io.c)
target_link_libraries(test_key_io PRIVATE libclock)
target_include_directories(test_key_io PRIVATE include)
add_test(NAME key_io COMMAND test_key_io)
