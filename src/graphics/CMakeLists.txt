include(FetchContent)

# gecko_add_shaders() lives with the Graphics module — anything that uses
# Graphics is the only thing that needs to embed SPIR-V blobs. The function
# is global once defined, so any CMakeLists processed after src/graphics can
# call it (examples, downstream apps via find_package(Gecko), etc.).
include(GeckoShaders)

# Vulkan SDK is a prerequisite for the Vulkan backend (see docs/build.md).
# find_package(Vulkan) works for both system/package-manager installs and the
# LunarG SDK (which sets $VULKAN_SDK).
find_package(Vulkan QUIET)

if(NOT Vulkan_FOUND)
  message(STATUS "")
  message(STATUS "Gecko: Vulkan SDK not found — Graphics module will only provide NullDevice.")
  message(STATUS "  To enable the Vulkan backend, install the SDK:")
  if(WIN32)
    message(STATUS "    MSYS2 UCRT64:  pacman -S mingw-w64-ucrt-x86_64-vulkan-devel")
    message(STATUS "    or LunarG:     https://vulkan.lunarg.com/")
  elseif(APPLE)
    message(STATUS "    LunarG SDK (bundles MoltenVK): https://vulkan.lunarg.com/")
  else()
    message(STATUS "    Debian/Ubuntu: sudo apt-get install libvulkan-dev vulkan-validationlayers spirv-tools")
    message(STATUS "    Fedora:        sudo dnf install vulkan-loader-devel vulkan-validation-layers spirv-tools")
    message(STATUS "    Arch:          sudo pacman -S vulkan-devel spirv-tools")
  endif()
  message(STATUS "")
else()
  message(STATUS "Gecko: Vulkan ${Vulkan_VERSION} — building Vulkan backend")

  # VMA — header-only, pinned tag. Only fetched when Vulkan is available so
  # offline/air-gapped configures without the SDK don't need network access.
  FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.2.1)
  FetchContent_MakeAvailable(VulkanMemoryAllocator)
endif()

add_library(Graphics STATIC)
add_library(Gecko::Graphics ALIAS Graphics)
set_target_properties(Graphics PROPERTIES OUTPUT_NAME "GeckoGraphics")

target_sources(Graphics PRIVATE
  graphics_module.cpp
  graphics_device.cpp
  private/null_device.cpp
)

target_include_directories(Graphics
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Graphics PUBLIC
  Gecko::Core
  Gecko::CoreServices
  Gecko::Platform
)

if(Vulkan_FOUND)
  target_sources(Graphics PRIVATE
    vulkan/vulkan_device.cpp
    vulkan/vulkan_command_list.cpp
    vulkan/vulkan_gpu_sampler.cpp
    vulkan/vulkan_surface.cpp
  )
  target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN)
  target_link_libraries(Graphics PRIVATE Vulkan::Vulkan)
  target_include_directories(Graphics PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan
  )
  # Treat the third-party VulkanMemoryAllocator header as a system include
  # so its warnings (C4100/C4127/C4189 on MSVC, etc.) don't trip /WX.
  target_include_directories(Graphics SYSTEM PRIVATE
    ${vulkanmemoryallocator_SOURCE_DIR}/include
  )

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11)
    if(X11_FOUND)
      target_sources(Graphics PRIVATE vulkan/linux/vulkan_xlib_surface.cpp)
      target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_XLIB)
      target_link_libraries(Graphics PRIVATE X11::X11)
    endif()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(WAYLAND_CLIENT QUIET wayland-client)
      if(WAYLAND_CLIENT_FOUND)
        target_sources(Graphics PRIVATE vulkan/linux/vulkan_wayland_surface.cpp)
        target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_WAYLAND)
        target_include_directories(Graphics PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
        target_link_libraries(Graphics PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
      endif()
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(Graphics PRIVATE vulkan/win32/vulkan_win32_surface.cpp)
    target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_WIN32)
  endif()
endif()

install(TARGETS Graphics
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/graphics
  DESTINATION include/gecko)
