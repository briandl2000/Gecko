add_executable(core_tests
  test_hash.cpp
  test_labels.cpp
  test_bit.cpp
  test_containers.cpp
  test_time.cpp
  test_random.cpp
  test_thread.cpp
  test_ptr.cpp
  test_string_view.cpp
  test_version.cpp
  test_memory.cpp
  test_events.cpp
  test_services.cpp
)

target_link_libraries(core_tests
  PRIVATE
    Gecko::Core
    Gecko::CoreServices
    Gecko::Runtime
    Catch2::Catch2WithMain
)

target_include_directories(core_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(core_tests PRIVATE -Wno-pedantic)
endif()
