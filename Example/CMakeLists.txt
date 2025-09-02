set(EXAMPLE_NAME GeckoExample)
project(EXAMPLE_NAME)

add_executable("${EXAMPLE_NAME}" 
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" 
    "src/ExampleComputePass.cpp" 
    "src/NodeBasedScene.cpp" 
    "src/GLTFSceneLoader.cpp" 
    "src/FlatHierarchyScene.cpp"
    "src/ThirdPartyBuilds/GLTFBuild.cpp")

target_compile_definitions("${EXAMPLE_NAME}" PUBLIC WORKING_DIR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/WorkingDir/")

target_link_libraries("${EXAMPLE_NAME}" PUBLIC "${GECKO}")

target_include_directories("${EXAMPLE_NAME}" PUBLIC "./Include")
target_include_directories("${EXAMPLE_NAME}" PUBLIC "../Include")
target_include_directories("${EXAMPLE_NAME}" PUBLIC "../ThirdParty/glm")
target_include_directories("${EXAMPLE_NAME}" PUBLIC "../ThirdParty/tinygltf")
