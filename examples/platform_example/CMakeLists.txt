add_executable(platform_example
  src/main.cpp
  src/App.cpp
)

target_link_libraries(platform_example
  PRIVATE Gecko::Core Gecko::Runtime Gecko::Platform
)
