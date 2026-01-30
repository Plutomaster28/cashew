cmake_minimum_required(VERSION 3.20)
project(Cashew VERSION 0.1.0 LANGUAGES CXX)

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
option(CASHEW_BUILD_EXAMPLES "Build examples" ON)
option(CASHEW_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(CASHEW_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(CASHEW_USE_LIBSODIUM "Use libsodium for crypto" ON)

# Sanitizers
if(CASHEW_ENABLE_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(CASHEW_ENABLE_TSAN AND NOT MSVC)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Dependencies
find_package(Threads REQUIRED)

# Crypto library (prefer libsodium)
if(CASHEW_USE_LIBSODIUM)
    find_package(sodium REQUIRED)
    set(CASHEW_CRYPTO_LIB sodium)
    add_definitions(-DCASHEW_USE_LIBSODIUM)
else()
    find_package(OpenSSL REQUIRED)
    set(CASHEW_CRYPTO_LIB OpenSSL::SSL OpenSSL::Crypto)
    add_definitions(-DCASHEW_USE_OPENSSL)
endif()

# Other dependencies - these would typically be found or added as submodules
# For now, we'll specify them as required
# In practice, use vcpkg or Conan for dependency management

# Logging library
# find_package(spdlog REQUIRED)

# JSON library
# find_package(nlohmann_json REQUIRED)

# Testing framework
if(CASHEW_BUILD_TESTS)
    enable_testing()
    # find_package(Catch2 REQUIRED)
endif()

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
    add_subdirectory(benchmarks)
endif()

if(CASHEW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY ${CMAKE_BINARY_DIR}/include/ DESTINATION include)

# Summary
message(STATUS "=== Cashew Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Crypto library: ${CASHEW_CRYPTO_LIB}")
message(STATUS "Build tests: ${CASHEW_BUILD_TESTS}")
message(STATUS "Build benchmarks: ${CASHEW_BUILD_BENCHMARKS}")
message(STATUS "Build examples: ${CASHEW_BUILD_EXAMPLES}")
message(STATUS "AddressSanitizer: ${CASHEW_ENABLE_ASAN}")
message(STATUS "ThreadSanitizer: ${CASHEW_ENABLE_TSAN}")
message(STATUS "================================")
