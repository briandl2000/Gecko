add_executable(core_example
  src/main.cpp
  src/App.cpp
  src/Demos.cpp
)

target_link_libraries(core_example
  PRIVATE Gecko::Core Gecko::Runtime
)
