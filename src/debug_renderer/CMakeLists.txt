add_library(DebugRenderer STATIC)
add_library(Gecko::DebugRenderer ALIAS DebugRenderer)

target_sources(DebugRenderer
  PRIVATE
    debug_renderer_module.cpp
    debug_renderer_context.cpp
)

target_include_directories(DebugRenderer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(DebugRenderer
  PUBLIC
    Gecko::Core
    Gecko::CoreServices
    
    Gecko::Platform
    Gecko::Graphics
    Gecko::Math
)

gecko_add_shaders(
  TARGET    DebugRenderer
  NAMESPACE gecko::debug_renderer::shaders
  HEADER    shaders.h
  SHADERS
    debug_line.vert.hlsl=DebugLineVert
    debug_line.frag.hlsl=DebugLineFrag
)


install(TARGETS DebugRenderer
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/debug_renderer
  DESTINATION include/gecko)
