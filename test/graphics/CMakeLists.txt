cmake_minimum_required(VERSION 3.22)

add_executable(graphics_tests
  test_objects.cpp
  test_null_device.cpp
)

target_link_libraries(graphics_tests
  PRIVATE
    Gecko::Graphics
    Gecko::Platform
    Gecko::Runtime
    Gecko::Core
    Gecko::CoreServices
    Catch2::Catch2WithMain
)

target_include_directories(graphics_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${GECKO_TEST_COMMON_DIR}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(graphics_tests PRIVATE -Wno-pedantic)
endif()
