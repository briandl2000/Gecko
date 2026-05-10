cmake_minimum_required(VERSION 3.22)

project(Gecko VERSION 0.0.0 LANGUAGES C CXX)

# Optional SemVer prerelease identifier (repo-controlled).
# Examples: "alpha.1", "beta.2", "rc.1". Set to "" for a normal release.
set(GECKO_VERSION_PRERELEASE "alpha.3")

if(GECKO_VERSION_PRERELEASE STREQUAL "")
  set(GECKO_VERSION_FULL "${PROJECT_VERSION}")
else()
  set(GECKO_VERSION_FULL "${PROJECT_VERSION}-${GECKO_VERSION_PRERELEASE}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate version.h from template with real values stamped in
configure_file(
  "${CMAKE_SOURCE_DIR}/include/gecko/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/include/gecko/version.h"
  @ONLY)

# LTO is opt-in via GECKO_DIST. Local Release dev builds skip LTO so that
# linking stays fast (the user iterates in Release; full LTO only matters
# for shipped artefacts). CI's `package` job sets -DGECKO_DIST=ON.
# Note: previous gating on CMAKE_BUILD_TYPE was a no-op under Ninja
# Multi-Config (the variable is empty there), so LTO had silently never
# been applied -- now it is, but only when explicitly requested.
option(GECKO_DIST "Enable distribution-grade optimization (LTO etc.)" OFF)
if(GECKO_DIST)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
  message(STATUS "GECKO_DIST=ON: LTO enabled for Release/RelWithDebInfo")
endif()

# Enable ccache if available for faster rebuilds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# Auto-select a fast linker on GCC/Clang. mold > lld > default ld.bfd.
# On Linux a typical incremental rebuild spends most of its time linking
# 4 example exes + the shared lib; switching to lld/mold cuts that
# multi-second link wall-clock to a few hundred ms. Suppress with
# -DGECKO_FAST_LINKER=OFF if a downstream tool (e.g. perf/gdb) needs the
# default linker for some reason.
option(GECKO_FAST_LINKER "Use mold/lld when available (GCC/Clang only)" ON)
if(GECKO_FAST_LINKER AND NOT MSVC)
  find_program(GECKO_MOLD mold)
  find_program(GECKO_LLD ld.lld)
  if(GECKO_MOLD)
    add_link_options(-fuse-ld=mold)
    message(STATUS "Using mold linker: ${GECKO_MOLD}")
  elseif(GECKO_LLD)
    add_link_options(-fuse-ld=lld)
    message(STATUS "Using lld linker: ${GECKO_LLD}")
  endif()
endif()

# Compiler warnings — GCC and Clang share these flags.
# The MSVC path covers clang-cl or native MSVC if ever used.
if(MSVC)
    add_compile_options(
        /W4 /WX
        # C4201: nameless struct/union -- used intentionally in math vector types.
        /wd4201
        # C4324: structure padded due to __declspec(align()) -- expected for
        # cache-aligned PODs like LogMessage / ProfEvent.
        /wd4324
    )
    # Silence MSVC's "use _s suffixed" deprecations for portable C APIs
    # (getenv, fopen, sscanf, ...). We code to the portable Standard C
    # surface; the _s variants are not portable.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
else()
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

# On MinGW, copy GCC/C++ runtime DLLs to the output directory so
# executables work without MSYS2 on PATH. We deliberately avoid static
# linking because mixing static and dynamic runtimes across our shared
# library (CoreServices.dll) causes duplicate-symbol crashes.
if(MINGW)
    get_filename_component(_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_MINGW_RUNTIME_DLLS
        "${_MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
        "${_MINGW_BIN_DIR}/libstdc++-6.dll"
        "${_MINGW_BIN_DIR}/libwinpthread-1.dll"
    )
    add_custom_target(gecko_copy_mingw_runtime ALL
        COMMENT "Copying MinGW runtime DLLs to output directories"
    )
    foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${_cfg} _CFGU)
        foreach(_dll ${_MINGW_RUNTIME_DLLS})
            if(EXISTS "${_dll}")
                add_custom_command(TARGET gecko_copy_mingw_runtime POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFGU}}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}"
                        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFGU}}/"
                )
            endif()
        endforeach()
    endforeach()
    foreach(_dll ${_MINGW_RUNTIME_DLLS})
        if(NOT EXISTS "${_dll}")
            message(WARNING "MinGW runtime DLL not found: ${_dll}")
        endif()
    endforeach()
endif()

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
