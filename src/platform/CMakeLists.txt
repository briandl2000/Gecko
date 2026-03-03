add_library(Platform STATIC)
add_library(Gecko::Platform ALIAS Platform)

target_sources(Platform
  PRIVATE
    null_windows_interface.cpp
    platform_context.cpp
    platform_module.cpp
    windows_interface.cpp
)

if(UNIX AND NOT APPLE)
  target_compile_definitions(Platform PUBLIC GECKO_PLATFORM_LINUX)

  find_package(X11)
  if(X11_FOUND)
    target_sources(Platform PRIVATE
      linux/x11_windows_interface.cpp
    )
    if(TARGET X11::X11)
      target_link_libraries(Platform PRIVATE X11::X11)
    else()
      target_include_directories(Platform PRIVATE ${X11_INCLUDE_DIR})
      target_link_libraries(Platform PRIVATE ${X11_LIBRARIES})
    endif()
    target_compile_definitions(Platform PRIVATE GECKO_PLATFORM_LINUX_X11)
  endif()

  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(Wayland QUIET wayland-client)
    if(Wayland_FOUND)
      target_compile_definitions(Platform PRIVATE GECKO_PLATFORM_LINUX_WAYLAND)
      message(STATUS "Gecko: Wayland client found – GECKO_PLATFORM_LINUX_WAYLAND enabled")
    endif()
  endif()
elseif(WIN32)
  target_sources(Platform PRIVATE
  )
  target_compile_definitions(Platform PUBLIC GECKO_PLATFORM_WINDOWS)
endif()

target_include_directories(Platform
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(Platform PUBLIC GECKO_PLATFORM_API)

target_link_libraries(Platform PUBLIC Gecko::Core Gecko::CoreServices)

install(TARGETS Platform
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/platform
  DESTINATION include/gecko)
