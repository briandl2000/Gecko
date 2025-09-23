add_executable(platform_example 
  src/main.cpp
)

target_link_libraries(platform_example
  PRIVATE Gecko::Core Gecko::Platform Gecko::Runtime
)
