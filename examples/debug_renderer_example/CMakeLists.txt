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
