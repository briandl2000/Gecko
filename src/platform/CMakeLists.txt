add_library(Platform STATIC)
add_library(Gecko::Platform ALIAS Platform)

target_sources(Platform
  PRIVATE
    window.cpp
    platform_module.cpp
)

if(UNIX AND NOT APPLE)
  target_sources(Platform PRIVATE linux/platform_context_linux.cpp)
  find_package(X11)
  if(X11_FOUND)
    target_sources(Platform PRIVATE linux/x11_window_backend.cpp)
    if(TARGET X11::X11)
      target_link_libraries(Platform PRIVATE X11::X11)
    else()
      target_include_directories(Platform PRIVATE ${X11_INCLUDE_DIR})
      target_link_libraries(Platform PRIVATE ${X11_LIBRARIES})
    endif()
    target_compile_definitions(Platform PRIVATE GECKO_PLATFORM_LINUX_X11)
  endif()
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
