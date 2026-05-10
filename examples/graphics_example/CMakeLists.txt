find_program(GECKO_GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GECKO_GLSLC)
  message(STATUS "graphics_example: glslc not found — example disabled. "
                 "Install the Vulkan SDK or set VULKAN_SDK to enable.")
  return()
endif()

add_executable(graphics_example
  src/main.cpp
  src/App.cpp
)

target_link_libraries(graphics_example
  PRIVATE
    Gecko::Core
    Gecko::Runtime
    Gecko::Platform
    Gecko::Graphics
)

# Precompile the heavy headers so changing App.cpp body is a sub-second
# rebuild. The PCH is shared between main.cpp and App.cpp.
target_precompile_headers(graphics_example PRIVATE
  <chrono>
  <cmath>
  <cstdlib>
  <optional>
  <utility>
  <gecko/core/engine.h>
  <gecko/core/labels.h>
  <gecko/core/ptr.h>
  <gecko/core/scope.h>
  <gecko/core/services.h>
  <gecko/core/services/events.h>
  <gecko/core/services/log.h>
  <gecko/core/services/memory.h>
  <gecko/core/services/modules.h>
  <gecko/core/types.h>
  <gecko/core/utility/thread.h>
  <gecko/core/version.h>
  <gecko/graphics/gpu_profiler.h>
  <gecko/graphics/graphics_device.h>
  <gecko/graphics/graphics_module.h>
  <gecko/platform/platform_events.h>
  <gecko/platform/platform_module.h>
  <gecko/platform/windows_interface.h>
  <gecko/runtime/runtime_module.h>
  <gecko/runtime/standard_log_sinks.h>
  <gecko/runtime/tracking_allocator.h>
)

gecko_add_shaders(
  TARGET    graphics_example
  NAMESPACE gecko::graphics_example::shaders
  HEADER    shaders.h
  SHADERS
    triangle.vert.hlsl=TriangleVert
    triangle.frag.hlsl=TriangleFrag
    fullscreen.vert.hlsl=FullscreenVert
    fullscreen.frag.hlsl=FullscreenFrag
    plasma.comp.hlsl=PlasmaComp
)
