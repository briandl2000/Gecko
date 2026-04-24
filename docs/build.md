# Build Guide

## Prerequisites

Install everything in this section **before** running `gk build`. The setup
script checks for these; missing pieces produce loud errors with the exact
install command for your platform.

### Toolchain

| Tool | Min version |
|------|-------------|
| GCC  | 15 (C++26) |
| CMake | 3.22 |
| Ninja | any |
| Python | 3.7 |

> **Why GCC only?** Gecko targets C++26 (`-std=c++2c`). As of mid-2026 GCC is
> the only compiler with broad C++26 support in a stable release. MSVC will
> be added once it ships C++26 core support — see
> [MSVC Migration Plan](#msvc-migration-plan).

#### Linux (Ubuntu / Debian)
```bash
sudo apt-get install gcc g++ ninja-build cmake python3 \
  libx11-dev libxext-dev libxrandr-dev libwayland-dev wayland-protocols
```

#### Linux (Fedora)
```bash
sudo dnf install gcc g++ ninja-build cmake python3 \
  libX11-devel libXext-devel libXrandr-devel wayland-devel wayland-protocols-devel
```

#### Linux (Arch)
```bash
sudo pacman -S gcc ninja cmake python libx11 libxext libxrandr wayland wayland-protocols
```

#### Windows (MSYS2 + MinGW-w64)
1. Install [MSYS2](https://www.msys2.org/).
2. Open the **UCRT64** terminal (not MSYS or MINGW64).
3. `pacman -S mingw-w64-ucrt-x86_64-{gcc,ninja,cmake,gdb,python}`
4. Verify: `gcc --version` should show 15+.

All `gk` commands must run from the UCRT64 terminal.

#### macOS
```bash
brew install gcc ninja cmake python
```

### Vulkan SDK

The Graphics module's real backend is Vulkan 1.3 (dynamic rendering + sync2).
Without the SDK `CreateGraphicsDevice()` falls back to a `NullDevice` (all
calls are no-ops) — useful for CI/headless builds, useless for rendering.

Gecko uses the standard CMake `find_package(Vulkan)`, which picks up both
system packages and the LunarG SDK (via `$VULKAN_SDK`). When the SDK is
missing, CMake prints the exact install command for your platform.

| Platform | Install |
|----------|---------|
| Ubuntu / Debian | `sudo apt-get install libvulkan-dev vulkan-validationlayers spirv-tools` |
| Fedora | `sudo dnf install vulkan-loader-devel vulkan-validation-layers spirv-tools` |
| Arch | `sudo pacman -S vulkan-devel spirv-tools` |
| MSYS2 UCRT64 | `pacman -S mingw-w64-ucrt-x86_64-vulkan-devel` |
| macOS | [LunarG SDK for macOS](https://vulkan.lunarg.com/) (bundles MoltenVK) |
| Windows (non-MSYS2) | [LunarG SDK](https://vulkan.lunarg.com/) |

**Shader compiler:** `glslc` ships with every SDK above. Examples that load
shaders (e.g. `graphics_example`) compile HLSL → SPIR-V at build time via
`glslc`. A missing `glslc` disables shader compilation for that example.

**Validation layers:** Debug builds enable `VK_LAYER_KHRONOS_validation` when
it is installed. Shipping builds don't require it.

## Setup

```bash
source scripts/setup.sh    # Linux / macOS / MSYS2 UCRT64
```

This creates the `gk` shell function, detects GCC, and runs the initial CMake
configure step. Re-run any time `CMakePresets.json` or prerequisites change.

## `gk` CLI

```bash
gk build                 # Debug build
gk build release         # Release build
gk build all             # Both
gk test                  # Build + run unit tests (Debug)
gk test --build-only     # Build tests without running
gk run <example>         # Build + run an example
gk format                # clang-format all sources
gk clean                 # Wipe out/ and reconfigure
gk package [local|dev|release]
```

## Manual CMake

```bash
cmake --preset debug
cmake --build out/build --config Debug
cmake --install out/build --prefix /your/prefix --config Release
```

## Output Structure

```
out/
├── build/<PlatformID>/   # CMake internals
└── <PlatformID>/
    ├── bin/{Debug,Release}/
    ├── lib/{Debug,Release}/
    └── package/
```

`<PlatformID>` is `<OS>-<arch>`, e.g. `Linux-x86_64`, `Windows-x86_64`.

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `GECKO_BUILD_EXAMPLES` | ON | Build example applications |
| `GECKO_BUILD_TESTS` | OFF | Build unit tests (auto-enabled by `gk test`) |
| `GECKO_OVERRIDE_NEW` | OFF | Route global `new`/`delete` through Gecko's allocator |

## VS Code Debugging

Pre-configured launch targets in `.vscode/launch.json`:

- **Local: Debug example** — GDB on Linux (pick Debug/Release + target)

Set breakpoints, press `F5`, pick a target. The Local target builds via
`gk: build` before launching.

## MinGW Runtime DLLs

On Windows the build copies `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, and
`libwinpthread-1.dll` alongside each executable so nothing depends on MSYS2
being on `PATH`.

## MSVC Migration Plan

1. **Now:** GCC 15 on Linux + Windows (MSYS2). Code is written to be
   MSVC-compatible where practical:
   - Use `_WIN32` / `GECKO_PLATFORM_WINDOWS`, not `_MSC_VER`
   - Use `<share.h>`, not `<corecrt_share.h>`
   - Use CMake `target_link_libraries`, not `#pragma comment(lib, ...)`
   - Guard GCC-only builtins with `#ifdef` fallbacks
2. **When MSVC ships C++26:** Add it as an alternate Windows compiler.

## VS Code Tasks & Debugging

`.vscode/tasks.json` provides `gk: build debug`, `gk: build release`,
`gk: test debug`, `gk: test release`, and `gk: format`. Default build
task is `gk: build debug` (`Ctrl+Shift+B`).

`.vscode/launch.json` defines flat per-OS-per-config launch entries.
Pick the one for your OS + configuration; the target (example or test
binary) is chosen via a single `debugTarget` picker on launch.

| Launch config | What it runs |
|---|---|
| **Linux Debug** / **Linux Release** | Native `gdb` against `out/Linux-x86_64/bin/{Debug,Release}/`. Uses `LD_LIBRARY_PATH` so the app finds `libCoreServices.so`. Prelaunch: `gk: build {debug,release}`. |
| **Windows Debug** / **Windows Release** | MSYS2 UCRT64 `gdb.exe` against `C:\Gecko\{debug,release}\`. Prelaunch builds on the Windows machine and mirrors artefacts there — see below. |

### Windows network-drive workaround

A common Gecko setup is: the workspace lives on a Linux or NAS machine,
and a Windows machine on the same LAN mounts it as a network drive
(e.g. `Z:\`). SmartScreen refuses to launch unsigned `.exe` files from
network drives. To work around this, the Windows launch configs build
into `Z:\out\Windows-x86_64\bin\{Debug,Release}\` as usual, then
mirror the artefacts to a plain local path — `C:\Gecko\debug\` or
`C:\Gecko\release\` — before debugging. Handled by
[`scripts/copy_debug.py`](../scripts/copy_debug.py), which is a no-op
on Linux/macOS so the same `.vscode/` works everywhere.

