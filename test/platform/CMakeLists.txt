add_executable(platform_tests
  test_window_types.cpp
  test_platform_context.cpp
  test_platform_io.cpp
  test_threading.cpp
  test_input.cpp
  test_clipboard.cpp
)

target_link_libraries(platform_tests
  PRIVATE
    Gecko::Platform
    Gecko::Runtime
    Gecko::Core
    Gecko::CoreServices
    Catch2::Catch2WithMain
)

target_include_directories(platform_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${GECKO_TEST_COMMON_DIR}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(platform_tests PRIVATE -Wno-pedantic)
endif()

# ── Feature tests (require live display) ─────────────────────────────

add_executable(platform_feature_tests
  feature_monitors.cpp
  feature_windows.cpp
  feature_input.cpp
)

target_link_libraries(platform_feature_tests
  PRIVATE
    Gecko::Platform
    Gecko::Runtime
    Gecko::Core
    Gecko::CoreServices
    Catch2::Catch2WithMain
)

target_include_directories(platform_feature_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${GECKO_TEST_COMMON_DIR}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(platform_feature_tests PRIVATE -Wno-pedantic)
endif()