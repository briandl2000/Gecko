add_library(GeckoCore INTERFACE)
add_library(Gecko::Core ALIAS GeckoCore)

target_include_directories(GeckoCore INTERFACE
 $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
 $<INSTALL_INTERFACE:include>)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/core DESTINATION include/gecko)
install(TARGETS GeckoCore EXPORT GeckoTargets)
