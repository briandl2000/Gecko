# Changelog

All notable changes to Gecko will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Graphics module** — API-agnostic backend with `GraphicsDevice` abstract class
  - `CreateGraphicsDevice()` factory returns `Unique<GraphicsDevice>` (NullDevice by default)
  - Full GPU resource API: `Buffer`, `Texture`, `RenderTarget`, `GraphicsPipeline`, `ComputePipeline`, `Swapchain`
  - Multi-window swapchain support: `CreateSwapchain(NativeWindowHandle, SwapchainDesc)`, `ResizeSwapchain(Swapchain&)`, `Present(Swapchain&)`
  - `ICommandList` interface: render target binding, texture/buffer binding, draw/dispatch commands
  - `NullDevice` + `NullCommandList` no-op implementations for headless operation
  - C++26 features: `[[nodiscard("reason")]]` on all Create* methods, `= delete("reason")` on non-copyable types, `std::span` for data upload APIs
  - `constexpr FormatSizeInBytes(DataFormat)` and `CalculateNumberOfMips(u32, u32)`
  - 31 unit tests covering all descriptor `IsValid()` methods and null device smoke tests
- **graphics_example** — demonstrates window + swapchain creation, frame loop with resize handling and ESC/close events

### Changed
- CI: main branch releases now use the CHANGELOG section for the release description instead of auto-generated commit notes
- CI: dev build releases no longer include a description
- Allocator decoupled from `Services`. Use `SetAllocator(IAllocator*)` / `ResetAllocator()` for lifecycle and `Allocator()` (returns reference) for access. `services.Allocator` and `GetAllocator()` removed.
- **Module-published services** — modules now declare what they `Publishes()` and `Requires()`; `Engine::Create({...})` starts them in topological order. Replaces the `Services` struct, `Install/UninstallServices`, and `GECKO_BOOT`/`GECKO_SHUTDOWN`. `runtime::CoreServicesModule` publishes the four foundational services (`IJobSystem`, `IProfiler`, `ILogger`, `IEventBus`); `Engine` itself owns the registry and lives in the `GeckoCoreServices` shared library.

## [0.0.0-alpha.2]

### Added
- **Platform windowing** — full window lifecycle management (create, resize, move, minimize, maximize, fullscreen, close)
- **Win32 window backend** — native Windows windowing with per-monitor DPI awareness
- **Wayland window backend** — xdg-shell based windowing with resize constraints, decorations, and fullscreen support
- **X11 window backend** — Xlib-based windowing for Linux
- **Monitor system** — platform-specific monitor backends for Linux (X11 + Wayland) and Windows
- **Platform event bus** — event dispatch for platform-level events (monitors, windows)
- **Rect2D** structure and corresponding tests
- **Unit tests** — core, platform, runtime, and math test modules (Catch2)
- **Feature tests** — visible window tests gated behind `[.visible]` tag
- **Cross-platform build infrastructure** — platform-separated build/output directories

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
