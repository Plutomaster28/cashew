cmake_minimum_required(VERSION 3.20)
project(Cashew VERSION 0.1.0 LANGUAGES C CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform detection
if(WIN32)
    message(STATUS "Building for Windows")
    add_definitions(-DCASHEW_PLATFORM_WINDOWS)
    
    # Detect MSYS2/MINGW
    if(MINGW)
        message(STATUS "Using MSYS2/MinGW toolchain")
    endif()
elseif(UNIX AND NOT APPLE)
    message(STATUS "Building for Linux")
    add_definitions(-DCASHEW_PLATFORM_LINUX)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Debug/Release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DCASHEW_DEBUG)
    if(NOT MSVC)
        add_compile_options(-g -O0)
    endif()
else()
    if(NOT MSVC)
        add_compile_options(-O3 -march=native)
    endif()
endif()

# Options
option(CASHEW_BUILD_TESTS "Build tests" ON)
option(CASHEW_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(CASHEW_BUILD_EXAMPLES "Build examples" OFF)
option(CASHEW_ENABLE_ASAN "Enable AddressSanitizer" OFF)

# Add official BLAKE3 library
add_subdirectory(third_party/BLAKE3/c)

# Find dependencies (MSYS2/system packages)
find_package(PkgConfig REQUIRED)

# libsodium
pkg_check_modules(sodium REQUIRED IMPORTED_TARGET libsodium)

# spdlog
find_package(spdlog REQUIRED)

# nlohmann-json
find_package(nlohmann_json REQUIRED)

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Dependencies
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Subdirectories
add_subdirectory(src)

if(CASHEW_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(CASHEW_BUILD_BENCHMARKS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
        add_subdirectory(benchmarks)
    else()
        message(WARNING "Benchmarks enabled but directory not found, skipping")
    endif()
endif()

if(CASHEW_BUILD_EXAMPLES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
        add_subdirectory(examples)
    else()
        message(WARNING "Examples enabled but directory not found, skipping")
    endif()
endif()

# Installation
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY ${CMAKE_BINARY_DIR}/include/ DESTINATION include)

# Summary
message(STATUS "=== Cashew Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests: ${CASHEW_BUILD_TESTS}")
message(STATUS "Build benchmarks: ${CASHEW_BUILD_BENCHMARKS}")
message(STATUS "Build examples: ${CASHEW_BUILD_EXAMPLES}")
message(STATUS "AddressSanitizer: ${CASHEW_ENABLE_ASAN}")
message(STATUS "================================")
