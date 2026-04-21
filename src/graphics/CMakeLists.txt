include(FetchContent)

FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.2.1)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

find_package(Vulkan)

add_library(Graphics STATIC)
add_library(Gecko::Graphics ALIAS Graphics)

target_sources(Graphics
  PRIVATE
    graphics_module.cpp
    graphics_device.cpp
    private/null_device.cpp
)

if(Vulkan_FOUND)
  target_sources(Graphics PRIVATE
      vulkan/vulkan_vma.cpp
      vulkan/vulkan_device.cpp
      vulkan/vulkan_command_list.cpp
  )
  target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN)
  target_link_libraries(Graphics PRIVATE Vulkan::Vulkan)
  target_include_directories(Graphics PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/vulkan
      ${vulkanmemoryallocator_SOURCE_DIR}/include
  )
endif()

target_include_directories(Graphics
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Graphics
  PUBLIC
    Gecko::Core
    Gecko::CoreServices
    Gecko::Platform
)

install(TARGETS Graphics
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/graphics
  DESTINATION include/gecko)
