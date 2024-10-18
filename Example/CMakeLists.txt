set(EXAMPLE_NAME GeckoExample)
project(EXAMPLE_NAME )

add_executable("${EXAMPLE_NAME}" "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

set_property(TARGET "${EXAMPLE_NAME}" PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries("${EXAMPLE_NAME}" PUBLIC "${GECKO}")

target_include_directories("${EXAMPLE_NAME}" PUBLIC "../Include")
target_include_directories("${EXAMPLE_NAME}" PUBLIC "../ThirdParty/glm")