add_executable(platform_example 
  main.cpp
)

target_link_libraries(platform_example
  PRIVATE Gecko::Core Gecko::Platform Gecko::Runtime
)
