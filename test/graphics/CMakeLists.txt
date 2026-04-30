add_executable(graphics_tests
  test_graphics_types.cpp
  test_null_device.cpp
  test_graphics_module.cpp
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

# ── Feature tests (full Engine + module stack with NullDevice) ───────

add_executable(graphics_feature_tests
  feature_graphics.cpp
)

target_link_libraries(graphics_feature_tests
  PRIVATE
    Gecko::Graphics
    Gecko::Platform
    Gecko::Runtime
    Gecko::Core
    Gecko::CoreServices
    Catch2::Catch2WithMain
)

target_include_directories(graphics_feature_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${GECKO_TEST_COMMON_DIR}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(graphics_feature_tests PRIVATE -Wno-pedantic)
endif()

# Mirror Graphics' GECKO_GRAPHICS_VULKAN gate so feature tests can
# compile-out backend-specific paths when Vulkan isn't available.
# find_package() result from src/graphics/ does not propagate across
# add_subdirectory scopes, so re-detect here.
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_compile_definitions(graphics_feature_tests PRIVATE GECKO_GRAPHICS_VULKAN)
endif()
