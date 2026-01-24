add_library(__MODULE_CAMEL__ STATIC)
add_library(Gecko::__MODULE_CAMEL__ ALIAS __MODULE_CAMEL__)

target_sources(__MODULE_CAMEL__
  PRIVATE
    __MODULE___module.cpp
)

target_include_directories(__MODULE_CAMEL__
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(__MODULE_CAMEL__
  PUBLIC
    Gecko::Core
    Gecko::CoreServices
    __MODULE_DEP_TARGETS__
)

install(TARGETS __MODULE_CAMEL__
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/__MODULE__
  DESTINATION include/gecko)
