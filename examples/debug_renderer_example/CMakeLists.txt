find_program(GECKO_GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GECKO_GLSLC)
  message(STATUS "debug_renderer_example: glslc not found - example disabled. "
                 "Install the Vulkan SDK or set VULKAN_SDK to enable.")
  return()
endif()

add_executable(debug_renderer_example
  src/main.cpp
  src/App.cpp
)

target_link_libraries(debug_renderer_example
  PRIVATE
    Gecko::Core
    Gecko::Runtime
    Gecko::Platform
    Gecko::Graphics
)

gecko_add_shaders(
  TARGET    debug_renderer_example
  NAMESPACE app::debug_renderer_example::shaders
  HEADER    shaders.h
  SHADERS
    debug_line.vert.hlsl=DebugLineVert
    debug_line.frag.hlsl=DebugLineFrag
)
