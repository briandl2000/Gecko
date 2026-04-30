# Changelog

All notable changes to Gecko will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **`gecko::AllocatorScope`** — RAII helper in `core/services/memory.h` that wraps `SetAllocator()` / `ResetAllocator()`. Replaces the per-example `AllocatorInstaller` boilerplate with a one-liner. Header-only, `[[nodiscard]] explicit operator bool()` reports whether `Init()` succeeded.
- **`gecko::runtime::StandardLogSinks`** — RAII helper that owns a `ConsoleLogSink` + `FileLogSink`, attaches them to the active logger on construction, and unregisters them on destruction. Default file path is `log.txt`; default level is `LogLevel::Info`. Engaged after `Engine::Create({...})` so it sees the real logger, not the null fallback.
- **`RuntimeModule` default-built logger is now thread-safe** — the `ImmediateLogger` instance owned by a default-constructed `RuntimeModule` has `SetThreadSafe(true)` enabled, since the job system and any GPU sampler thread will hit it from worker threads. Custom loggers passed via `Backends` are unaffected.
- **Shader build helper** — `gecko_add_shaders(TARGET NAMESPACE HEADER SHADERS [SOURCE_DIR])` CMake function compiles HLSL → SPIR-V via `glslc` and emits a single header that exposes each blob as `inline constexpr unsigned char` arrays via C++26 `#embed`. Stage is inferred from the filename (`.vert.hlsl`, `.frag.hlsl`, `.comp.hlsl`, etc.); variable names default to the capitalized basename with explicit `=Name` overrides. Auto-included for downstream `find_package(Gecko)` consumers. `examples/graphics_example` migrated to it (130-line CMakeLists → 30 lines, byte-identical generated header). See `docs/shader_pipeline.md`.
- **Platform IO** — `gecko::platform` namespace functions: `Exists`, `Stat`, `Read`, `Map`, `Write`, `AtomicWrite`, `OpenWrite`, `CreateDir`, `Remove`, `IterateDir`, plus well-known paths (`ExePath`, `WorkingDir`, `UserDataDir`). Move-only `ReadResult`, `MappedFile`, `DirIter`, and `FileWriter` handles. Linux + Win32 backends.
- **Platform threading** — namespace functions for thread naming, sleep, yield, and hardware concurrency (Linux + Win32).
- **Platform input** — `IInput` service with key/mouse state, press/release edges, scroll, mouse position, focused/hovered window, and UTF-8 typed text. Auto `NewFrame()` on event-bus drain. `MockInput` test fake under `test/common/`.
- **Platform clipboard** — `Get`/`Set` text on X11 and Win32 (Wayland TODO).
- **Platform terminal** — `Print` / `PrintLine` UTF-8 API with optional ANSI color; TTY detection suppresses color when redirected. Win32 forces CP_UTF8 and enables VT mode.
- **`platform_example`** — polls `IInput` every frame and logs changes; `--visible` test flag for IInput showcase.
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
- **Profiler v2** — leveled zones (`Always` / `Normal` / `Detailed`) with runtime cap, sink-side level filtering, watch-scope rolling averages, `SetTraceEnabled`, sample-rate, `DumpStats`, ring-overflow / reentrant-drop diagnostics. `GECKO_GPU_SCOPE_*` macros plus auto GPU zones via `ICommandList::AttachGpuSampler`.
- **GPU sampler** — `IGpuSampler` interface and `VulkanGpuSampler` impl using per-frame timestamp pools. GPU timestamps are rebased to CPU `vkQueueSubmit` time so Perfetto traces line up under their CPU submit calls.
- **AsyncTraceProfilerSink** — Chrome-trace JSON writer on a worker thread with batched writes, `SetMinLevel` filter, `thread_name` metadata, drains on dtor.
- **`docs/profiling.md`** — usage guide, GPU sampler timing model, multi-queue notes, and explanation of why GPU zones can legitimately appear before their CPU submit.
- **ABI lint** — `scripts/lint_abi.py` scans CoreServices interface headers (and `engine.h`) for `std::` types and is wired into `gk test` so the boundary stays clean. Suppression marker `// abi-ok: <reason>` (or `// abi-ok-begin` / `// abi-ok-end` for header-only template regions).

### Changed
- **Examples migrated to the new boot ergonomics.** `app_skeleton`, `core_example`, `graphics_example`, `platform_example`, and the `gk new-example` template now use `AllocatorScope`, `RuntimeModule()` default ctor, and (where applicable) `StandardLogSinks`. Net ~220 fewer lines of boot ceremony across the example tree, leaving the example logic itself.
- **Build system: portable C++23, no fixed compiler.** CMake auto-detects the toolchain (GCC on Linux, MSVC 19.44+ on Windows, aarch64-linux-gnu-g++ for `gk-pi`). Removed the MSYS2/MinGW dependency on Windows; the `gk` workflow now runs from a Developer PowerShell for VS 2022 (or any PowerShell that dot-sources `scripts/setup.ps1`).
- **CoreServices ABI cleaned up.** All virtuals on `IAllocator`, `IJobSystem`, `IProfiler`, `ILogger`, `IEventBus`, `IModule`, `IProfilerSink`, ... now take only primitives, raw pointers, `const char*`, Gecko PODs, or `gecko::Span<T>` — no `std::string`, `std::span`, `std::function`, or other STL types whose layout differs between MSVC, libstdc++, and libc++.
- **`gecko::Span<T>`** — new POD `{T*, usize}` view in `include/gecko/core/span.h` that replaces `std::span` on every CoreServices virtual signature. Trivially copyable, layout-stable across toolchains, with both PascalCase and `std::span`-style accessors.
- **`IJobSystem` is ABI-stable.** `Submit` virtuals now take a `JobFn { Invoke, Free, User }` POD payload; the convenience `Submit<F>(callable, ...)` overload boxes any lambda/`std::function` in the caller's TU. Stateless callables avoid heap allocation entirely. `JobFunction = std::function<void()>` typedef removed.
- **Shader pipeline portable.** Replaced the C++26 `#embed` path with a CMake helper that emits a 16-byte-per-line `0x..,` header from each `.spv`, so the engine builds against any C++23-conforming compiler.
- `IWindowsBackend` and `IMonitorsBackend` promoted to services; `PlatformModule` now supports caller-owned backend injection.
- Runtime trace/log sinks (`TraceFileSink`, `CrashSafeTraceProfilerSink`, `TraceWriter`) migrated to `FileWriter`.
- `DirIter::Next()` returns `::std::optional<DirEntry>` (was out-reference).
- Constants drop the `k` prefix per coding standard.
- CI: main branch releases now use the CHANGELOG section for the release description instead of auto-generated commit notes
- CI: dev build releases no longer include a description
- Allocator decoupled from `Services`. Use `SetAllocator(IAllocator*)` / `ResetAllocator()` for lifecycle and `Allocator()` (returns reference) for access. `services.Allocator` and `GetAllocator()` removed.
- **Module-published services** — modules now declare what they `Publishes()` and `Requires()`; `Engine::Create({...})` starts them in topological order. Replaces the `Services` struct, `Install/UninstallServices`, and `GECKO_BOOT`/`GECKO_SHUTDOWN`. `runtime::RuntimeModule` publishes the four foundational services (`IJobSystem`, `IProfiler`, `ILogger`, `IEventBus`); `Engine` itself owns the registry, which now lives in the `GeckoCore` static library so registry tests/dependents need not link `GeckoCoreServices`.
- **Uniform module ctor convention** — `RuntimeModule`, `PlatformModule`, and `GraphicsModule` now all support three forms: a default ctor that owns sensible production defaults internally, a `Backends` ctor for partial injection (null fields get the default; non-null are caller-owned), and (where applicable) a full reference ctor for explicit injection of every slot.
- `GraphicsModule` rewritten to follow the publish/require module pattern: `GraphicsConfig { BackendKind, EnableValidation, ... }` plus `Backends { GraphicsDevice* }` for caller-owned device injection. The previous standalone `CreateGraphicsDevice()` factory is no longer required for boot — examples now construct a `GraphicsModule` and read the device via `GetGraphicsDevice()`.
- Made the code use Doxygen-style comments in the public API for clarity.
- Updated the example projects to use more of an oop style and make it easier to explore.

### Fixed
- **MSVC build** — Win32 sources now include `<Windows.h>` before `<processthreadsapi.h>` / `<ShlObj.h>` / `<fileapi.h>` (wrapped in `// clang-format off` so `IncludeBlocks: Regroup` does not undo the order). Resolves `winnt.h "No Target Architecture"` errors on `cl.exe`.
- `CreateDir(recursive=true)` no longer writes one byte past `std::string`/`std::wstring` `size()` when terminating path segments (Linux + Win32).
- Trace sink `WriteFmt` helpers no longer truncate at 1024 bytes — heap fallback via two-pass `vsnprintf` ensures full JSON records.
- `ConsoleLogSink` heap path no longer overruns its `std::string` buffer by one byte.

### Removed
- `NullInput` class (replaced by event-driven default).

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
