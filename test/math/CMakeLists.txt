add_executable(math_tests
  test_vector.cpp
  test_matrix.cpp
  test_aabb.cpp
  test_quat.cpp
)

target_link_libraries(math_tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(math_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Suppress pedantic warnings for GCC/Clang only (not MSVC)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(math_tests PRIVATE -Wno-pedantic)
endif()

# Discover and register tests with CTest
include(Catch)
catch_discover_tests(math_tests)
