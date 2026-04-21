add_library(Graphics STATIC)
add_library(Gecko::Graphics ALIAS Graphics)

target_sources(Graphics
  PRIVATE
    graphics_module.cpp
    graphics_device.cpp
    null_device.cpp
)

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
