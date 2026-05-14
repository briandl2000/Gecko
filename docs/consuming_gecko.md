# Consuming Gecko in Your Own CMake Project

This guide shows the **copy-paste** way to use a tagged Gecko release in your
own CMake project. Just pick the release tag you want from
[Gecko releases](https://github.com/briandl2000/Gecko/releases) and drop it
into the snippet below — CMake handles download, extraction, and linking.

## TL;DR — Three files

```
MyGame/
├── CMakeLists.txt
├── cmake/
│   └── FetchGecko.cmake
└── src/
    └── main.cpp
```

### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fill in the Gecko release tag you want to use. See:
#   https://github.com/briandl2000/Gecko/releases
set(GECKO_VERSION "0.0.0-alpha.3")   # <-- change me
include(cmake/FetchGecko.cmake)

add_executable(MyGame src/main.cpp)
target_link_libraries(MyGame PRIVATE
    Gecko::Core
    Gecko::CoreServices
    Gecko::Math
    Gecko::Runtime
    Gecko::Platform
    Gecko::Graphics     # remove if you don't need the renderer
)

# On Linux, the executable needs to look in its own directory for the
# Gecko .so files we copy next to it below.
if(UNIX AND NOT APPLE)
    set_target_properties(MyGame PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# Copy Gecko's runtime shared library (CoreServices) next to the
# executable so it runs in-place on both Linux and Windows.
#
# Note: $<TARGET_RUNTIME_DLLS:...> is empty on non-DLL platforms (Linux
# and macOS), so it does NOT cover the libGeckoCoreServices.so case --
# use $<TARGET_FILE:...> against the shared imported target instead.
add_custom_command(TARGET MyGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Gecko::CoreServices>"
        "$<TARGET_FILE_DIR:MyGame>"
    VERBATIM
)

# Copy compile_commands.json to the project root so clangd / your editor
# can find it without extra config.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(copy_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${PROJECT_SOURCE_DIR}/compile_commands.json"
        BYPRODUCTS "${PROJECT_SOURCE_DIR}/compile_commands.json"
        VERBATIM
    )
endif()
```

### `cmake/FetchGecko.cmake`

Copy this file verbatim — it works for any Gecko release tag.

```cmake
# Downloads and extracts a tagged Gecko release, then calls find_package().
#
# Required input:
#   GECKO_VERSION  -- e.g. "0.0.0-alpha.3" or "0.0.0-alpha.3-dev.20260512183646"
#                     (without the leading "v")
#
# After this file is included, the following targets are available:
#   Gecko::Core, Gecko::CoreServices, Gecko::Math,
#   Gecko::Runtime, Gecko::Platform, Gecko::Graphics

if(NOT DEFINED GECKO_VERSION)
    message(FATAL_ERROR "FetchGecko: set GECKO_VERSION before including this file")
endif()

set(GECKO_TAG "v${GECKO_VERSION}")

if(WIN32)
    set(_gecko_platform "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_gecko_platform "linux")
else()
    message(FATAL_ERROR "Gecko binary releases are only published for Windows and Linux.")
endif()

set(_gecko_pkg     "gecko-${GECKO_VERSION}-${_gecko_platform}.zip")
set(_gecko_url     "https://github.com/briandl2000/Gecko/releases/download/${GECKO_TAG}/${_gecko_pkg}")
set(_gecko_deps    "${CMAKE_BINARY_DIR}/_deps")
set(_gecko_zip     "${_gecko_deps}/${_gecko_pkg}")
set(_gecko_extract "${_gecko_deps}/gecko-${GECKO_VERSION}-${_gecko_platform}")

file(MAKE_DIRECTORY "${_gecko_deps}")

if(NOT EXISTS "${_gecko_zip}")
    message(STATUS "Downloading Gecko: ${_gecko_url}")
    file(DOWNLOAD "${_gecko_url}" "${_gecko_zip}"
        SHOW_PROGRESS
        TLS_VERIFY ON
        STATUS _gecko_dl_status)
    list(GET _gecko_dl_status 0 _gecko_dl_code)
    if(NOT _gecko_dl_code EQUAL 0)
        file(REMOVE "${_gecko_zip}")
        message(FATAL_ERROR "Failed to download Gecko: ${_gecko_dl_status}")
    endif()
endif()

if(NOT EXISTS "${_gecko_extract}/lib/cmake/Gecko/GeckoConfig.cmake")
    message(STATUS "Extracting Gecko to: ${_gecko_extract}")
    file(MAKE_DIRECTORY "${_gecko_extract}")
    file(ARCHIVE_EXTRACT INPUT "${_gecko_zip}" DESTINATION "${_gecko_extract}")
endif()

# The zip extracts to <extract>/<install tree>. Releases >= 0.0.0-alpha.4
# zip the install contents directly (flat: <extract>/lib/cmake/Gecko/...),
# while older releases wrapped them in an extra gecko-<version>/ folder.
# Look for GeckoConfig.cmake in either layout.
file(GLOB_RECURSE _gecko_config "${_gecko_extract}/*GeckoConfig.cmake")
if(NOT _gecko_config)
    message(FATAL_ERROR "GeckoConfig.cmake not found under ${_gecko_extract}")
endif()
list(GET _gecko_config 0 _gecko_config)
get_filename_component(Gecko_DIR "${_gecko_config}" DIRECTORY)

# Put Gecko's root (containing bin/ lib/ include/) on CMAKE_PREFIX_PATH so
# find_dependency() calls inside GeckoConfig.cmake can resolve normally.
get_filename_component(_gecko_root "${Gecko_DIR}/../../.." ABSOLUTE)
list(APPEND CMAKE_PREFIX_PATH "${_gecko_root}")

find_package(Gecko CONFIG REQUIRED)
```

### `src/main.cpp`

A minimal program just to verify the link works:

```cpp
#include <gecko/version.h>

int main() {
    return 0;
}
```

## Building it

```bash
cmake -S . -B build
cmake --build build
./build/MyGame
```

## System dependencies

Gecko's Linux package links against X11, Wayland, XKB, and (optionally) Vulkan
through CMake imported targets, so the **same Linux zip works on any distro**
that has the dev packages installed.

| Distro | Command |
|--------|---------|
| Arch | `sudo pacman -S libx11 libxrandr wayland libxkbcommon vulkan-icd-loader` |
| Ubuntu/Debian | `sudo apt-get install libx11-dev libxrandr-dev libwayland-dev libxkbcommon-dev libvulkan-dev` |
| Fedora | `sudo dnf install libX11-devel libXrandr-devel wayland-devel libxkbcommon-devel vulkan-loader-devel` |

On Windows, no system dependencies beyond the Vulkan runtime are required;
runtime DLLs ship inside the Gecko zip and are copied next to your exe by
the `$<TARGET_RUNTIME_DLLS:...>` POST_BUILD step.

## Picking a tag

- **Stable releases**: `v0.1.0`, `v1.0.0`, ... — published from `main`.
- **Pre-releases**: `v0.0.0-alpha.N` / `v0.0.0-beta.N` — also from `main`.
- **Dev snapshots**: `v0.0.0-alpha.N-dev.<timestamp>` — published from `dev`
  on every merge. Useful for trying the bleeding edge; not guaranteed stable.

Find the exact tag you want on the
[Releases page](https://github.com/briandl2000/Gecko/releases) and drop the
part **after** the leading `v` into `GECKO_VERSION`.

## Pinning by SHA-256 (optional)

If you want reproducible builds, add the zip's SHA-256 from the release page
and extend `file(DOWNLOAD ...)` in `FetchGecko.cmake`:

```cmake
file(DOWNLOAD "${_gecko_url}" "${_gecko_zip}"
    EXPECTED_HASH SHA256=<paste-hash-here>
    SHOW_PROGRESS
    TLS_VERIFY ON
    STATUS _gecko_dl_status)
```
