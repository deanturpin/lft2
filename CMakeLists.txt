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

add_executable(lft2 main.cxx)
