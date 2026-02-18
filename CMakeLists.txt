cmake_minimum_required(VERSION 3.28)
project(lft2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Generates build/compile_commands.json for clangd/IDE

# Compiler flags for gcc 15
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -O1
    -march=native
)

add_library(barlib STATIC src/bar.cxx)
target_include_directories(barlib PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_library(fixlib STATIC src/fix.cxx)
target_include_directories(fixlib PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test src/test.cxx)

target_include_directories(test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
add_executable(profile src/profile.cxx)
add_executable(evaluate src/evaluate.cxx)
add_executable(backtest src/backtest.cxx)
add_executable(exits src/exits.cxx)
add_executable(entries src/entries.cxx)

target_link_libraries(backtest PRIVATE barlib)
target_link_libraries(evaluate PRIVATE barlib)
target_link_libraries(exits    PRIVATE barlib fixlib)
target_link_libraries(entries  PRIVATE barlib fixlib)

target_include_directories(profile PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Profile target gets debug, profiling, and coverage flags
target_compile_options(profile PRIVATE
    -g2                      # Maximum debug information
    -fno-omit-frame-pointer  # Keep frame pointers for better stack traces
    --coverage               # gcov instrumentation
)
target_link_options(profile PRIVATE --coverage)

# Add gprof profiling flags on Linux (not supported on macOS)
if(UNIX AND NOT APPLE)
    target_compile_options(profile PRIVATE -pg)
    target_link_options(profile PRIVATE -pg)
endif()

target_include_directories(evaluate PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(backtest PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(exits PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(entries PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
