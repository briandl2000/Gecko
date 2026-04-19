cmake_minimum_required(VERSION 3.22)

project(Gecko VERSION 0.0.0 LANGUAGES C CXX)

# Enforce Clang compiler (either Clang or clang-cl on Windows)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR 
    "Gecko requires Clang compiler but detected: ${CMAKE_CXX_COMPILER_ID}\n"
    "Please install LLVM/Clang:\n"
    "  Windows: Visual Studio Installer > Individual Components > 'C++ Clang Compiler'\n"
    "  Ubuntu:  sudo apt-get install clang\n"
    "  macOS:   brew install llvm\n"
    "Then delete the out/build directory and reconfigure.")
endif()

# Optional SemVer prerelease identifier (repo-controlled).
# Examples: "alpha.1", "beta.2", "rc.1". Set to "" for a normal release.
set(GECKO_VERSION_PRERELEASE "alpha.2")

if(GECKO_VERSION_PRERELEASE STREQUAL "")
  set(GECKO_VERSION_FULL "${PROJECT_VERSION}")
else()
  set(GECKO_VERSION_FULL "${PROJECT_VERSION}-${GECKO_VERSION_PRERELEASE}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate version.h from template with real values stamped in
configure_file(
  "${CMAKE_SOURCE_DIR}/include/gecko/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/include/gecko/version.h"
  @ONLY)

# Enable LTO only for Release builds (Debug builds are much faster without it)
if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Enable ccache if available for faster rebuilds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# Compiler flags - Clang-only, consistent across platforms
# clang-cl uses MSVC-style flags, regular clang uses GCC-style
if(MSVC)
    # clang-cl on Windows (still sets MSVC=true but uses Clang frontend)
    add_compile_options(/W4 /WX)
else()
    # clang on Linux/macOS
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter)
endif()

# All outputs go to out/<PlatformID>/ at project root, separated by platform
# CMake internals stay in out/build/<PlatformID>/
# Normalize architecture names across platforms (AMD64 → x86_64, ARM64 → aarch64)
set(_gecko_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(_gecko_arch STREQUAL "AMD64")
  set(_gecko_arch "x86_64")
elseif(_gecko_arch STREQUAL "ARM64")
  set(_gecko_arch "aarch64")
endif()
set(GECKO_PLATFORM_ID "${CMAKE_SYSTEM_NAME}-${_gecko_arch}")
if(GECKO_OUTPUT_DIR)
  set(GECKO_OUT_DIR "${GECKO_OUTPUT_DIR}")
else()
  set(GECKO_OUT_DIR "${CMAKE_SOURCE_DIR}/out/${GECKO_PLATFORM_ID}")
endif()
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFGU)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${GECKO_OUT_DIR}/bin/${cfg}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${GECKO_OUT_DIR}/bin/${cfg}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${GECKO_OUT_DIR}/lib/${cfg}")
endforeach()

option(GECKO_BUILD_EXAMPLES "Build examples" ON)
option(GECKO_BUILD_TESTS "Build tests" OFF)
option(GECKO_OVERRIDE_NEW "Override global operator new/delete to route through Gecko allocator" OFF)

# Copy the compilation database to the repo root on build for editor tooling.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  add_custom_target(gecko_copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
    BYPRODUCTS "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Copy compile_commands.json to project root"
  )
endif()

# CMake package/export naming (kept consistent with the project name).
set(GECKO_EXPORT_SET_NAME "${PROJECT_NAME}Targets")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(src)

if (GECKO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Test targets require GECKO_BUILD_TESTS=ON (set by setup scripts and gk test).
# EXCLUDE_FROM_ALL keeps them out of the default 'gk build' target.
add_subdirectory(test EXCLUDE_FROM_ALL)

include(CMakePackageConfigHelpers)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/install.cmake)
