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

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) #link time optimization

# Enable all warnings and treat warnings as errors
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter)
endif()

set(_OUT ${CMAKE_BINARY_DIR}/out)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFGU)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} ${_OUT}/bin/${cfg})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFGU} ${_OUT}/bin/${cfg})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFGU} ${_OUT}/lib/${cfg})
endforeach()

option(GECKO_BUILD_EXAMPLES "Build examples" ON)

# CMake package/export naming (kept consistent with the project name).
set(GECKO_EXPORT_SET_NAME "${PROJECT_NAME}Targets")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/runtime)

if (GECKO_BUILD_EXAMPLES)
  add_subdirectory(examples/core_example)
  add_subdirectory(examples/platform_example)
  add_subdirectory(examples/app_skeleton)
endif()

include(CMakePackageConfigHelpers)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/install.cmake)
