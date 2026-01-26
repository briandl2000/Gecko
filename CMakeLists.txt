cmake_minimum_required(VERSION 3.22)

project(Gecko VERSION 0.0.0 LANGUAGES C CXX)

# Optional SemVer prerelease identifier (repo-controlled).
# Examples: "alpha.1", "beta.2", "rc.1". Set to "" for a normal release.
set(GECKO_VERSION_PRERELEASE "alpha.0")

if(GECKO_VERSION_PRERELEASE STREQUAL "")
  set(GECKO_VERSION_FULL "${PROJECT_VERSION}")
else()
  set(GECKO_VERSION_FULL "${PROJECT_VERSION}-${GECKO_VERSION_PRERELEASE}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Enable all warnings and treat warnings as errors
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter)
endif()

# All outputs go to out/ at project root
# CMake internals stay in out/build/
set(GECKO_OUT_DIR "${CMAKE_SOURCE_DIR}/out")
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFGU)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${GECKO_OUT_DIR}/bin/${cfg}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${GECKO_OUT_DIR}/bin/${cfg}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${GECKO_OUT_DIR}/lib/${cfg}")
endforeach()

option(GECKO_BUILD_EXAMPLES "Build examples" ON)
option(GECKO_BUILD_TESTS "Build tests" OFF)

# CMake package/export naming (kept consistent with the project name).
set(GECKO_EXPORT_SET_NAME "${PROJECT_NAME}Targets")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(src)

if (GECKO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Always add test directory so targets exist, but they won't build unless requested
add_subdirectory(test)

include(CMakePackageConfigHelpers)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/install.cmake)
