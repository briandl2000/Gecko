add_executable(runtime_tests
  test_tracking_allocator.cpp
  test_immediate_logger.cpp
  test_thread_pool_job_system.cpp
  test_event_bus.cpp
  test_module_registry.cpp
  test_ring_profiler.cpp
  test_async_trace_profiler_sink.cpp
)

target_link_libraries(runtime_tests
  PRIVATE
    Gecko::Runtime
    Gecko::Platform
    Gecko::Core
    Gecko::CoreServices
    Catch2::Catch2WithMain
)

target_include_directories(runtime_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(runtime_tests PRIVATE -Wno-pedantic)
endif()

# ── Feature tests (real subsystems, multi-thread workloads) ──────────

add_executable(runtime_feature_tests
  feature_profiler.cpp
)

target_link_libraries(runtime_feature_tests
  PRIVATE
    Gecko::Runtime
    Gecko::Platform
    Gecko::Core
    Gecko::CoreServices
    Catch2::Catch2WithMain
)

target_include_directories(runtime_feature_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(runtime_feature_tests PRIVATE -Wno-pedantic)
endif()