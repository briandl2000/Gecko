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

if(GECKO_OVERRIDE_NEW)
  target_compile_definitions(Core PRIVATE GECKO_OVERRIDE_NEW)
endif()

target_sources(CoreServices
  PRIVATE
    services.cpp
)

target_link_libraries(CoreServices PUBLIC Gecko::Core)

target_include_directories(Core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

# ── OS / platform feature macros ────────────────────────────────────────────
# Exposed as PUBLIC so every target that links Gecko::Core receives them
# automatically – no header inclusion required.
if(WIN32)
  target_compile_definitions(Core PUBLIC GECKO_PLATFORM_WINDOWS=1)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(Core PUBLIC GECKO_PLATFORM_LINUX=1)
elseif(APPLE)
  target_compile_definitions(Core PUBLIC GECKO_PLATFORM_APPLE=1)
endif()

install(TARGETS Core CoreServices
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/core DESTINATION include/gecko)
install(FILES ${CMAKE_BINARY_DIR}/generated/include/gecko/version.h DESTINATION include/gecko)
