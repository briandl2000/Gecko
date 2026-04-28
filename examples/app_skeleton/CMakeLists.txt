add_executable(app_skeleton
  src/main.cpp
  src/App.cpp
)

target_link_libraries(app_skeleton
  PRIVATE Gecko::Core Gecko::Runtime Gecko::Platform
)
