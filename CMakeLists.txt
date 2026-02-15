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
add_executable(profile src/profile.cxx)
add_executable(trade src/trade.cxx)

target_include_directories(lft2 PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(profile PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Profile target gets debug and profiling flags
target_compile_options(profile PRIVATE
    -g3                      # Maximum debug information
    -fno-omit-frame-pointer  # Keep frame pointers for better stack traces
)

# Add gprof profiling flags on Linux (not supported on macOS)
if(UNIX AND NOT APPLE)
    target_compile_options(profile PRIVATE -pg)
    target_link_options(profile PRIVATE -pg)
endif()

target_include_directories(trade PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
