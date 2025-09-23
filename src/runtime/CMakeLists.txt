add_library(GeckoRuntime)
add_library(Gecko::Runtime ALIAS GeckoRuntime)

target_sources(GeckoRuntime
  PRIVATE
    console_sink.cpp
    immediate_logger.cpp
    override_new.cpp
    ring_logger.cpp
    ring_profiler.cpp
    trace_writer.cpp
    tracking_allocator.cpp
)

target_include_directories(GeckoPlatform
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(GeckoRuntime PRIVATE GECKO_OVERRIDE_NEW)

target_compile_definitions(GeckoRuntime PUBLIC GECKO_PLATFORM_API)

target_link_libraries(GeckoRuntime PUBLIC Gecko::Core)

install(TARGETS GeckoRuntime
  EXPORT GeckoTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/runtime
  DESTINATION include/gecko)
