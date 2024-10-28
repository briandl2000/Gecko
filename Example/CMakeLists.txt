set(EXAMPLE_NAME GeckoExample)
project(EXAMPLE_NAME)

add_executable("${EXAMPLE_NAME}" "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

if(PRODUCTION_BUILD)
	target_compile_definitions("${GECKO}" PUBLIC WORKING_DIR_PATH="") 
	target_compile_definitions("${GECKO}" PUBLIC PRODUCTION_BUILD=1) 
else()
	target_compile_definitions("${GECKO}" PUBLIC WORKING_DIR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/WorkingDir/")
	target_compile_definitions("${GECKO}" PUBLIC PRODUCTION_BUILD=0)
endif()

target_link_libraries("${EXAMPLE_NAME}" PUBLIC "${GECKO}")

target_include_directories("${EXAMPLE_NAME}" PUBLIC "../Include")
target_include_directories("${EXAMPLE_NAME}" PUBLIC "../ThirdParty/glm")

