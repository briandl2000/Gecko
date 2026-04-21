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
