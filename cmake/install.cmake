install(EXPORT GeckoTargets
  NAMESPACE gecko::
  DESTINATION lib/cmake/Gecko)

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/GeckoConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/GeckoConfig.cmake
  INSTALL_DESTINATION lib/cmake/Gecko)

install(FILES 
  ${CMAKE_CURRENT_BINARY_DIR}/GeckoConfig.cmake
  DESTINATION lib/cmake/Gecko)

