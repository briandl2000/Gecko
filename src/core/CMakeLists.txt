add_library(Core)
add_library(Gecko::Core ALIAS Core)

target_sources(Core
  PRIVATE
    modules.cpp
    services.cpp
    thread.cpp
    time.cpp
    random.cpp
)

target_include_directories(Core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(Core
  PUBLIC
    GECKO_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    GECKO_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    GECKO_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    GECKO_VERSION_STRING="${PROJECT_VERSION}"
    GECKO_VERSION_PRERELEASE="${GECKO_VERSION_PRERELEASE}"
    GECKO_VERSION_FULL_STRING="${GECKO_VERSION_FULL}")

install(TARGETS Core
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/core DESTINATION include/gecko)
