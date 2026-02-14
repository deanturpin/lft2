cmake_minimum_required(VERSION 3.28)
project(lft2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for gcc 15
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -O1
    -march=native
)

add_executable(lft2 src/main.cxx src/test.cxx)
add_executable(backtest src/backtest.cxx)

target_include_directories(lft2 PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(backtest PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
