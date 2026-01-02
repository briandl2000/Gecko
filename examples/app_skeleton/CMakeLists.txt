add_executable(app_skeleton
  src/main.cpp
)

target_link_libraries(app_skeleton
  PRIVATE Gecko::Core Gecko::Runtime Gecko::Platform
)
