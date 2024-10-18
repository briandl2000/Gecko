cmake_minimum_required(VERSION 3.22)

set(GECKO Gecko)

project(GECKO VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library("${GECKO}")

target_sources("${GECKO}" PRIVATE ${MY_SOURCES})

target_include_directories(Gecko PUBLIC "./Include")
target_include_directories(Gecko PRIVATE "./src")
target_include_directories(Gecko PUBLIC "./ThirdParty/glm")
target_include_directories(Gecko PUBLIC "./ThirdParty/stb_image")
target_include_directories(Gecko PUBLIC "./ThirdParty/imgui")
target_include_directories(Gecko PUBLIC "./ThirdParty/tinygltf")

add_subdirectory(Example)