add_library(Platform STATIC)
add_library(Gecko::Platform ALIAS Platform)
set_target_properties(Platform PROPERTIES OUTPUT_NAME "GeckoPlatform")

target_sources(Platform
  PRIVATE
    monitors_interface.cpp
    platform_config.cpp
    platform_io.cpp
    platform_module.cpp
    input.cpp
    clipboard.cpp
    terminal.cpp
    window_event_input.cpp
    windows_interface.cpp

    private/null_monitors_backend.cpp
    private/null_windows_interface.cpp
)

if(UNIX AND NOT APPLE)
  target_sources(Platform PRIVATE
    linux/platform_io_linux.cpp
    linux/threading_linux.cpp
  )
  find_package(X11)
  if(X11_FOUND)
    target_sources(Platform PRIVATE
      linux/x11_windows_interface.cpp
      linux/x11_monitors_backend.cpp
    )
    # Always prefer imported targets (X11::X11, X11::Xrandr) over raw
    # ${X11_*_LIB} variables: the raw variables hold absolute, distro-
    # specific paths (e.g. /usr/lib/x86_64-linux-gnu/libXrandr.so on
    # Ubuntu) that, if baked into the exported targets, make the released
    # Gecko package unusable on other distros (Arch, Fedora, ...).
    # Imported targets get re-resolved by find_dependency(X11) in the
    # consumer's GeckoConfig.cmake load.
    target_link_libraries(Platform PRIVATE X11::X11)

    if(NOT X11_Xrandr_FOUND OR NOT TARGET X11::Xrandr)
      message(FATAL_ERROR
        "Gecko: libXrandr is required on Linux but was not found. "
        "Install libxrandr-dev (Ubuntu/Debian) or libxrandr (Arch).")
    endif()
    target_link_libraries(Platform PRIVATE X11::Xrandr)

    target_compile_definitions(Platform PRIVATE GECKO_PLATFORM_LINUX_X11)
  endif()

  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    # Use IMPORTED_TARGET so the exported package references named imported
    # targets (PkgConfig::Wayland, ...) rather than absolute library paths
    # that are baked in at build time. This is what makes the installed
    # Gecko package relocatable across Linux distros (Ubuntu/Arch/Fedora).
    pkg_check_modules(Wayland QUIET IMPORTED_TARGET wayland-client)
    if(Wayland_FOUND)
      # ── Wayland protocol code generation ──────────────────────────
      find_program(WAYLAND_SCANNER wayland-scanner)
      pkg_check_modules(XkbCommon QUIET IMPORTED_TARGET xkbcommon)
      pkg_check_modules(WaylandCursor QUIET IMPORTED_TARGET wayland-cursor)

      if(WAYLAND_SCANNER)
        execute_process(
          COMMAND pkg-config --variable=pkgdatadir wayland-protocols
          OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(NOT WAYLAND_PROTOCOLS_DIR)
          message(WARNING "wayland-protocols pkgdatadir not found — "
                          "xdg-shell/xdg-decoration generation disabled")
        endif()

        set(_wl_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/wayland-gen")
        file(MAKE_DIRECTORY ${_wl_gen_dir})

        if(WAYLAND_PROTOCOLS_DIR)
          # xdg-shell (stable)
          set(_xdg_shell_xml
              "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
          add_custom_command(
            OUTPUT "${_wl_gen_dir}/xdg-shell-client-protocol.h"
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${_xdg_shell_xml}
                    "${_wl_gen_dir}/xdg-shell-client-protocol.h"
            DEPENDS ${_xdg_shell_xml}
            COMMENT "Generating xdg-shell client header"
          )
          add_custom_command(
            OUTPUT "${_wl_gen_dir}/xdg-shell-protocol.c"
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${_xdg_shell_xml}
                    "${_wl_gen_dir}/xdg-shell-protocol.c"
            DEPENDS ${_xdg_shell_xml}
            COMMENT "Generating xdg-shell protocol code"
          )

          # xdg-decoration (unstable v1)
          set(_xdg_deco_xml
              "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml")
          if(EXISTS "${_xdg_deco_xml}")
            add_custom_command(
              OUTPUT "${_wl_gen_dir}/xdg-decoration-client-protocol.h"
              COMMAND ${WAYLAND_SCANNER} client-header
                      ${_xdg_deco_xml}
                      "${_wl_gen_dir}/xdg-decoration-client-protocol.h"
              DEPENDS ${_xdg_deco_xml}
              COMMENT "Generating xdg-decoration client header"
            )
            add_custom_command(
              OUTPUT "${_wl_gen_dir}/xdg-decoration-protocol.c"
              COMMAND ${WAYLAND_SCANNER} private-code
                      ${_xdg_deco_xml}
                      "${_wl_gen_dir}/xdg-decoration-protocol.c"
              DEPENDS ${_xdg_deco_xml}
              COMMENT "Generating xdg-decoration protocol code"
            )
            set(_wl_deco_sources
              "${_wl_gen_dir}/xdg-decoration-protocol.c"
              "${_wl_gen_dir}/xdg-decoration-client-protocol.h"
            )
            target_compile_definitions(Platform PRIVATE GECKO_HAVE_XDG_DECORATION)
          else()
            message(STATUS "xdg-decoration-unstable-v1.xml not found — "
                           "decoration support disabled")
            set(_wl_deco_sources "")
          endif()

          target_sources(Platform PRIVATE
            linux/wayland_monitors_backend.cpp
            linux/wayland_windows_backend.cpp
            "${_wl_gen_dir}/xdg-shell-protocol.c"
            "${_wl_gen_dir}/xdg-shell-client-protocol.h"
            ${_wl_deco_sources}
          )
          target_include_directories(Platform PRIVATE ${_wl_gen_dir})
        else()
          target_sources(Platform PRIVATE
            linux/wayland_monitors_backend.cpp
          )
        endif()
      else()
        target_sources(Platform PRIVATE
          linux/wayland_monitors_backend.cpp
        )
      endif()

      target_link_libraries(Platform PRIVATE PkgConfig::Wayland)
      target_compile_definitions(Platform PRIVATE GECKO_PLATFORM_LINUX_WAYLAND)

      if(WaylandCursor_FOUND)
        target_link_libraries(Platform PRIVATE PkgConfig::WaylandCursor)
      endif()

      if(XkbCommon_FOUND)
        target_link_libraries(Platform PRIVATE PkgConfig::XkbCommon)
        target_compile_definitions(Platform PRIVATE GECKO_HAS_XKBCOMMON)
      endif()

      message(STATUS "Gecko: Wayland client found – GECKO_PLATFORM_LINUX_WAYLAND enabled")
    endif()
  endif()
elseif(WIN32)
  target_sources(Platform PRIVATE
    win32/win32_monitors_backend.cpp
    win32/win32_windows_backend.cpp
    win32/platform_io_win32.cpp
    win32/threading_win32.cpp
  )
  target_link_libraries(Platform PRIVATE winmm shcore user32 ole32)
endif()

target_include_directories(Platform
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(Platform PUBLIC GECKO_PLATFORM_API)

target_link_libraries(Platform PUBLIC Gecko::Core Gecko::CoreServices Gecko::Math)

install(TARGETS Platform
  EXPORT ${GECKO_EXPORT_SET_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/gecko/platform
  DESTINATION include/gecko)
