add_executable(__EXAMPLE__
  src/main.cpp
)

target_link_libraries(__EXAMPLE__
  PRIVATE
    Gecko::Core
    Gecko::Runtime
    __EXAMPLE_DEP_TARGETS__
)
