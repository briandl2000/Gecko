find_program(GECKO_GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GECKO_GLSLC)
  message(STATUS "bench_debug_renderer: glslc not found - bench disabled. "
                 "Install the Vulkan SDK or set VULKAN_SDK to enable.")
  return()
endif()

# Glob all test_*.cpp files so adding a new variation only requires
# dropping a new file alongside the others.
file(GLOB _BENCH_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

add_executable(bench_debug_renderer ${_BENCH_SRCS})

target_link_libraries(bench_debug_renderer
  PRIVATE
    Gecko::Bench
    Gecko::BenchMain
)
