find_program(GECKO_GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GECKO_GLSLC)
  message(STATUS "bench_debug_renderer: glslc not found - bench disabled. "
                 "Install the Vulkan SDK or set VULKAN_SDK to enable.")
  return()
endif()

add_executable(bench_debug_renderer bench_debug_renderer.cpp)

target_link_libraries(bench_debug_renderer
  PRIVATE
    Gecko::Bench
    Gecko::BenchMain
)
