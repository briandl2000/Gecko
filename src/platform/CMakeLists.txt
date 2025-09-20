add_library(GeckoPlatform)
add_library(Gecko::Platform ALIAS GeckoPlatform)

target_sources(GeckoPlatform
  PRIVATE
    platform_init.cpp
)

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
