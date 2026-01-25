add_executable(math_example
  src/main.cpp
)

target_link_libraries(math_example
  PRIVATE
    Gecko::Math
)
