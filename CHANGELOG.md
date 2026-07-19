# Changelog

Gecko development releases use the `v0.0.0-alpha.N` line.

## [Unreleased]

### Added

- One Python build driver with declarative per-project module descriptions and no third-party Python packages.
- One shared Gecko engine library linked by executables and plugins.
- A versioned `GeckoPlugin_GetApi` boundary for dynamically loaded plugins.
- Unity build entry points per module and one top-level `gecko_engine.cpp`.
- A complete `<gecko/gecko.h>` public umbrella header.
- Gecko-owned strings, arrays, spans, optional values, fixed maps, synchronization, and `{}` formatting.
- One process-wide virtual-memory allocator shared by every engine consumer.
- Concrete logger, profiler, job system, event bus, platform, and graphics systems owned by Gecko.
- Typed initialization results and assertion handling without exceptions or RTTI.
- A staged Debug/Release SDK containing public headers, libraries, debug symbols, and the build driver.
- An external-consumer check that rebuilds and runs the sandbox through the staged SDK.

### Changed

- C++ is used as a transparent C-like systems language: no standard-library runtime, exception flow, RTTI, injected service graphs, or RAII-driven architecture.
- Wayland remains the preferred Linux backend while X11 remains available for compatibility.
- Vulkan allocations are explicit and no longer use VMA.
- Logging and profiling use typed `{}` formatting rather than printf-style format strings.
- Version `0.0.0-alpha.4` is checked in directly and needs no configure step.
- Linux and Windows use the same build commands, module descriptions, shader declarations, and output layout.
- Shader compilation uses module-scoped `glslc` C initializers, with no custom embedding executable or runtime shader files.

### Fixed

- Engine and plugin allocations resolve to one allocator authority in the shared Gecko library.
- Unity build freshness checks include implementation files as well as headers.

### Removed

- CMake, shell/batch build duplication, Docker, tests, the old example collection, vendored third-party code, generated IDE state, and AI instruction/context files.
- The module registry and injectable runtime backend hierarchy.
- Standard-library containers, strings, formatting, threading, ownership, and I/O from engine source.

## [0.0.0-alpha.2]

### Added
- **Platform windowing** - full window lifecycle management (create, resize, move, minimize, maximize, fullscreen, close)
- **Win32 window backend** - native Windows windowing with per-monitor DPI awareness
- **Wayland window backend** - xdg-shell based windowing with resize constraints, decorations, and fullscreen support
- **X11 window backend** - Xlib-based windowing for Linux
- **Monitor system** - platform-specific monitor backends for Linux (X11 + Wayland) and Windows
- **Platform event bus** - event dispatch for platform-level events (monitors, windows)
- **Rect2D** structure and corresponding tests
- **Unit tests** - core, platform, runtime, and math test modules (Catch2)
- **Feature tests** - visible window tests gated behind `[.visible]` tag
- **Cross-platform build infrastructure** - platform-separated build/output directories

### Changed
- Platform API returns values instead of output references
- Event bus API renamed: Send/Dispatch convention
- Platform context refactored for cleaner backend selection
- Platform feature macros streamlined in CMakeLists (removed platform.h)
- `GECKO_PLATFORM_WINDOWS` macro replaces raw `_WIN32` checks
- Build scripts handle Windows network/mapped drives (UNC path safety)
- PR templates cleaned up with concise checklists
- Dev releases hidden from "latest" on GitHub, main releases use auto-generated notes
- Force use GCC now for c++26 features. Updated the build system.

### Fixed
- CI version extraction on Windows (portable sed-based extraction)
- Windows DLL loading during test discovery at build time
- IntelliSense not working due to compile_commands.json location
- Label scope macros use `__LINE__` for unique identifiers
- Xlib and Wayland availability checks for headless environments

## [0.0.0-alpha.1]

### Added
- Automated CI releases for both dev and main branches
- PR templates (feature and release) with checklists
- PR checks: version bump and changelog enforcement for main merges
- Consistent version strings across Linux/Windows packages
- CHANGELOG.md for tracking changes

### Fixed
- CI version extraction failing on Windows (`grep -oP` replaced with portable `sed`)
- Linux/Windows packages having mismatched timestamps

## [0.0.0-alpha.0]

### Added
- Core module: services, memory management, scope system, boot sequence
- Math module: vectors, matrices, quaternions, AABBs
- Platform module: windowing (X11 on Linux, Win32 on Windows)
- Runtime module: logging, profiling, job system, module registry, event bus
- CI pipeline with automated packaging and GitHub releases
- Cross-platform support (Linux + Windows) with Clang

### Changed
- Replaced `grep -oP` with portable `sed` in CI for Windows compatibility

### Fixed
- CI version extraction failing on Windows due to `grep -P` not supporting locale
