add_library(GeckoPlatform)
add_library(Gecko::Platform ALIAS GeckoPlatform)

target_sources(GeckoPlatform
  PRIVATE
    window.cpp
)

if(UNIX AND NOT APPLE)
  target_sources(GeckoPlatform PRIVATE linux/platform_context_linux.cpp)
  find_package(X11)
  if(X11_FOUND)
    target_sources(GeckoPlatform PRIVATE linux/x11_window_backend.cpp)
    if(TARGET X11::X11)
      target_link_libraries(GeckoPlatform PRIVATE X11::X11)
    else()
      target_include_directories(GeckoPlatform PRIVATE ${X11_INCLUDE_DIR})
      target_link_libraries(GeckoPlatform PRIVATE ${X11_LIBRARIES})
    endif()
    target_compile_definitions(GeckoPlatform PRIVATE GECKO_PLATFORM_LINUX_X11)
  endif()
endif()

target_include_directories(GeckoPlatform
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(GeckoPlatform PUBLIC GECKO_PLATFORM_API)

target_link_libraries(GeckoPlatform PUBLIC Gecko::Core)

install(TARGETS GeckoPlatform
  EXPORT GeckoTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/platform
  DESTINATION include/gecko)
