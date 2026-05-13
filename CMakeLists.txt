cmake_minimum_required(VERSION 3.20)
project(Clock C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(Clock
    src/main.c
    src/clk_clock.c
    src/clk_render.c
)

target_include_directories(Clock PRIVATE include)
target_link_libraries(Clock PRIVATE ncursesw)