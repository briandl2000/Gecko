add_library(Core STATIC)
add_library(Gecko::Core ALIAS Core)

# Enable position independent code for static lib that links into shared libs
set_target_properties(Core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(Core
  PRIVATE
    services/events.cpp
    services/jobs.cpp
    services/log.cpp
    services/memory.cpp
    services/profiler.cpp
    services/modules.cpp
    
    utility/thread.cpp
    utility/time.cpp
    utility/random.cpp

    overwrite_new.cpp
)

# CoreServices: SHARED library for service singletons
# Ensures singleton consistency across plugins/DLLs
add_library(CoreServices SHARED)
add_library(Gecko::CoreServices ALIAS CoreServices)

# Export all symbols on Windows to create the .lib import library
set_target_properties(CoreServices PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_compile_definitions(Core PRIVATE GECKO_OVERRIDE_NEW)

target_sources(CoreServices
  PRIVATE
    services.cpp
)

target_link_libraries(CoreServices PUBLIC Gecko::Core)

target_include_directories(Core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(Core
  PUBLIC
    GECKO_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    GECKO_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    GECKO_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    GECKO_VERSION_STRING="${PROJECT_VERSION}"
    GECKO_VERSION_PRERELEASE="${GECKO_VERSION_PRERELEASE}"
    GECKO_VERSION_FULL_STRING="${GECKO_VERSION_FULL}")

install(TARGETS Core CoreServices
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/core DESTINATION include/gecko)
