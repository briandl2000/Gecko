cmake_minimum_required(VERSION 3.22)

if(NOT GECKO_BUILD_TESTS)
  # Tests disabled - skip everything
  return()
endif()

# Fetch Catch2 testing framework
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# Enable CTest integration and add Catch2's CMake modules
include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# Output test binaries to tests/ subdirectory
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFGU)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${GECKO_OUT_DIR}/bin/${cfg}/tests")
endforeach()

# Add test subdirectories
add_subdirectory(math)
