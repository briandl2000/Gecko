include(FetchContent)

FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.2.1)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

find_package(Vulkan QUIET)

# Cross-compile path (e.g. aarch64 Raspberry Pi): no Vulkan SDK on the host
# for the target arch.  Fetch the arch-neutral Khronos headers and link
# against a prebuilt libvulkan.so.1 pulled from the target via
# `scripts/fetch_pi_vulkan.sh` → third_party/prebuilt/<platform>/.
if(NOT Vulkan_FOUND AND CMAKE_CROSSCOMPILING)
  set(_gk_prebuilt_dir "${CMAKE_SOURCE_DIR}/third_party/prebuilt/${GECKO_PLATFORM_ID}")
  set(_gk_vk_lib "${_gk_prebuilt_dir}/libvulkan.so.1")
  set(_gk_vk_headers "${CMAKE_SOURCE_DIR}/third_party/vulkan-headers/include")
  if(EXISTS "${_gk_vk_lib}" AND EXISTS "${_gk_vk_headers}/vulkan/vulkan.h")
    add_library(Vulkan::Vulkan SHARED IMPORTED)
    set_target_properties(Vulkan::Vulkan PROPERTIES
      IMPORTED_LOCATION             "${_gk_vk_lib}"
      IMPORTED_SONAME               "libvulkan.so.1"
      INTERFACE_INCLUDE_DIRECTORIES "${_gk_vk_headers}")
    set(Vulkan_FOUND TRUE)
    set(Vulkan_VERSION "cross/vendored")
    message(STATUS "Gecko: cross Vulkan — headers=${_gk_vk_headers} lib=${_gk_vk_lib}")
  else()
    message(STATUS "Gecko: cross Vulkan missing")
    if(NOT EXISTS "${_gk_vk_lib}")
      message(STATUS "  Missing lib: ${_gk_vk_lib}  (run scripts/fetch_pi_vulkan.sh)")
    endif()
    if(NOT EXISTS "${_gk_vk_headers}/vulkan/vulkan.h")
      message(STATUS "  Missing headers: ${_gk_vk_headers}  (run: git clone --depth 1 --branch vulkan-sdk-1.3.290.0 https://github.com/KhronosGroup/Vulkan-Headers third_party/vulkan-headers)")
    endif()
  endif()
endif()

if(Vulkan_FOUND)
  message(STATUS "Gecko: Vulkan ${Vulkan_VERSION} found — building Vulkan backend")
else()
  message(STATUS "Gecko: Vulkan not found — Graphics module will only provide NullDevice")
endif()

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
      vulkan/vulkan_device.cpp
      vulkan/vulkan_command_list.cpp
      vulkan/vulkan_surface.cpp
  )
  target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN)
  target_link_libraries(Graphics PRIVATE Vulkan::Vulkan)
  target_include_directories(Graphics PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/vulkan
      ${vulkanmemoryallocator_SOURCE_DIR}/include
  )

  # VMA and clang-in-cross-mode have a stricter default warning set than
  # native gcc; silence the ones that are purely third-party or cosmetic.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(Graphics PRIVATE
      -Wno-nullability-completeness
      -Wno-nullability-extension
      -Wno-unused-private-field)
  endif()

  # Per-OS surface backends
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Cross-compile path: use vendored aarch64 headers + prebuilt libs fetched
    # via scripts/fetch_pi_vulkan.sh.  We deliberately avoid the host's
    # /usr/include because that poisons the aarch64 toolchain's stdint/etc.
    if(CMAKE_CROSSCOMPILING AND EXISTS "${_gk_prebuilt_dir}")
      set(_gk_headers_dir "${CMAKE_SOURCE_DIR}/third_party/prebuilt/include")
      if(EXISTS "${_gk_prebuilt_dir}/libX11.so.6" AND EXISTS "${_gk_headers_dir}/X11/Xlib.h")
        target_sources(Graphics PRIVATE vulkan/linux/vulkan_xlib_surface.cpp)
        target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_XLIB)
        target_include_directories(Graphics PRIVATE "${_gk_headers_dir}")
        target_link_libraries(Graphics PRIVATE "${_gk_prebuilt_dir}/libX11.so.6")
        message(STATUS "Gecko: cross Xlib surface — vendored headers + prebuilt libX11")
      endif()
      if(EXISTS "${_gk_prebuilt_dir}/libwayland-client.so.0" AND EXISTS "${_gk_headers_dir}/wayland-client.h")
        target_sources(Graphics PRIVATE vulkan/linux/vulkan_wayland_surface.cpp)
        target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_WAYLAND)
        target_include_directories(Graphics PRIVATE "${_gk_headers_dir}")
        target_link_libraries(Graphics PRIVATE "${_gk_prebuilt_dir}/libwayland-client.so.0")
        message(STATUS "Gecko: cross Wayland surface — vendored headers + prebuilt libwayland-client")
      endif()
    else()
      find_package(PkgConfig)
      # X11 (Xlib) backend
      find_package(X11)
      if(X11_FOUND)
        target_sources(Graphics PRIVATE vulkan/linux/vulkan_xlib_surface.cpp)
        target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_XLIB)
        target_link_libraries(Graphics PRIVATE X11::X11)
      endif()
      # Wayland backend
      if(PKG_CONFIG_FOUND)
        pkg_check_modules(WAYLAND_CLIENT wayland-client)
        if(WAYLAND_CLIENT_FOUND)
          target_sources(Graphics PRIVATE vulkan/linux/vulkan_wayland_surface.cpp)
          target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_WAYLAND)
          target_include_directories(Graphics PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
          target_link_libraries(Graphics PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
        endif()
      endif()
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(Graphics PRIVATE vulkan/win32/vulkan_win32_surface.cpp)
    target_compile_definitions(Graphics PRIVATE GECKO_GRAPHICS_VULKAN_WIN32)
  endif()
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
