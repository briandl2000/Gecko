add_executable(platform_tests
  test_window_types.cpp
  test_platform_context.cpp
)

target_link_libraries(platform_tests
  PRIVATE
    Gecko::Platform
    Gecko::Runtime
    Gecko::Core
    Gecko::CoreServices
    Catch2::Catch2WithMain
)

target_include_directories(platform_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(platform_tests PRIVATE -Wno-pedantic)
endif()