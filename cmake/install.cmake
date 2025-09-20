install(EXPORT GeckoTargets
  NAMESPACE gecko::
  DESTINATION lib/cmake/gecko)

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/GeckoConfig.cmake.in
  ${CMAKE_SOURCE_DIR}/GeckoConfig.cmake
  INSTALL_DESTINATION lib/cmake/Gecko)

install(FILES 
  ${CMAKE_SOURCE_DIR}/GeckoConfig.cmake
  DESTINATION lib/cmake/Gecko)

