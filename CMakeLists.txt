cmake_minimum_required(VERSION 3.22)

project(GECKO VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) #link time optimization

set(_OUT ${CMAKE_BINARY_DIR}/out)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFGU)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} ${_OUT}/bin/${cfg})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFGU} ${_OUT}/bin/${cfg})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFGU} ${_OUT}/lib/${cfg})
endforeach()

option(GECKO_BUILD_EXAMPLES "Build examples" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/runtime)

if (GECKO_BUILD_EXAMPLES)
  add_subdirectory(examples/core_example)
  add_subdirectory(examples/platform_example)
endif()

include(CMakePackageConfigHelpers)
