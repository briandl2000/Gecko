add_library(GeckoCore)
add_library(Gecko::Core ALIAS GeckoCore)

target_sources(GeckoCore
  PRIVATE
    services.cpp
)

target_include_directories(GeckoCore
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

install(TARGETS GeckoCore
  EXPORT GeckoTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/core DESTINATION include/gecko)
