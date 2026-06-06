add_executable(math_tests
  test_scalar.cpp
  test_vector.cpp
  test_matrix.cpp
  test_geometry.cpp
  test_aabb.cpp
  test_quat.cpp
  test_rect.cpp
  test_transform.cpp
)

target_link_libraries(math_tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(math_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(math_tests PRIVATE -Wno-pedantic)
endif()
