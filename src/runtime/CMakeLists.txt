add_library(Runtime)
add_library(Gecko::Runtime ALIAS Runtime)

target_sources(Runtime
  PRIVATE
    console_log_sink.cpp
    crash_safe_trace_profiler_sink.cpp
    event_bus.cpp
    file_log_sink.cpp
    immediate_logger.cpp
    module_registry.cpp
    override_new.cpp
    ring_logger.cpp
    ring_profiler.cpp
    thread_pool_job_system.cpp
    trace_file_sink.cpp
    trace_writer.cpp
    tracking_allocator.cpp
    runtime_module.cpp
)

target_include_directories(Runtime
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(Runtime PRIVATE GECKO_OVERRIDE_NEW)

target_compile_definitions(Runtime PUBLIC GECKO_PLATFORM_API)

target_link_libraries(Runtime PUBLIC Gecko::Core)

install(TARGETS Runtime
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/runtime
  DESTINATION include/gecko)
