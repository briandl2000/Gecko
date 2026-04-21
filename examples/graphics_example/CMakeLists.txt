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
# Shaders are embedded at compile time via C++26 #embed — no runtime copy needed.
