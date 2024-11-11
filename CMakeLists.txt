cmake_minimum_required(VERSION 3.22)

project(GECKO VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) #link time optimization

if(MSVC) 
	add_compile_options(/arch:AVX2) #make sure SIMD optimizations take place
endif()

set(GECKO Gecko)

option(RELEASE "Make this a release build" OFF)
option(PRODUCTION_BUILD "Make this a production build" OFF)

add_library("${GECKO}")

if(PRODUCTION_BUILD)
	target_compile_definitions("${GECKO}" PUBLIC PRODUCTION_BUILD) 
endif()

if(RELEASE)
	target_compile_definitions("${GECKO}" PUBLIC RELEASE)
else()
	target_compile_definitions("${GECKO}" PUBLIC DEBUG)
endif()

if (WIN32)
	target_compile_definitions("${GECKO}" PUBLIC DIRECTX_12)
endif (WIN32)

if(MSVC) # If using the VS compiler...
	target_compile_definitions("${GECKO}" PUBLIC _CRT_SECURE_NO_WARNINGS)

	#set_property(TARGET "${GECKO}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug<$<CONFIG:Debug>:Debug>")
	#set_property(TARGET "${GECKO}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
endif()

if(MSVC)
	target_compile_options("${GECKO}" PRIVATE /W4 /WX)
else()
	target_compile_options("${GECKO}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
target_sources("${GECKO}" PRIVATE ${MY_SOURCES})

target_include_directories(Gecko PUBLIC "./Include")
target_include_directories(Gecko PRIVATE "./src")
target_include_directories(Gecko PUBLIC "./ThirdParty/glm")
target_include_directories(Gecko PUBLIC "./ThirdParty/stb_image")
target_include_directories(Gecko PUBLIC "./ThirdParty/imgui")
target_include_directories(Gecko PUBLIC "./ThirdParty/tinygltf")

add_subdirectory(Example)
