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
