add_executable(graphics_example
  src/main.cpp
)

target_link_libraries(graphics_example
  PRIVATE
    Gecko::Core
    Gecko::Runtime
    Gecko::Platform
    Gecko::Graphics
)

# Copy pre-compiled SPIR-V shaders next to the executable
set(_shader_src ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(_shader_dst $<TARGET_FILE_DIR:graphics_example>/shaders)

add_custom_command(TARGET graphics_example POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_shader_dst}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${_shader_src}/triangle.vert.spv
      ${_shader_src}/triangle.frag.spv
      ${_shader_dst}
  COMMENT "Copying shaders for graphics_example"
)
