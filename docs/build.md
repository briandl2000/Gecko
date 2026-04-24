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

## VS Code Tasks

Pre-configured tasks in `.vscode/tasks.json`: **gk build**, **gk test**,
**gk clean**. Run with `Ctrl+Shift+B` or the command palette.

## aarch64 cross-compile

Gecko ships a Docker image that produces Linux aarch64 artefacts from an
x86_64 host. It carries a `gcc-15-aarch64-linux-gnu` cross-toolchain and
arm64 multi-arch runtime libs (Vulkan, X11, Wayland). The compiler runs
natively on the host — no QEMU — so build speed matches a local x86_64
build. CI uses the same recipe; see
[`.github/workflows/ci.yml`](../.github/workflows/ci.yml).

```bash
docker build -t gecko-aarch64-dev -f docker/aarch64.Dockerfile .

docker run --rm -t \
    --user "$(id -u):$(id -g)" \
    -v "$PWD":/workspace -w /workspace \
    -e HOME=/tmp \
    -e GECKO_PLATFORM_ID=Linux-aarch64 \
    -e CMAKE_TOOLCHAIN_FILE=/workspace/docker/aarch64-toolchain.cmake \
    gecko-aarch64-dev \
    python3 scripts/cli.py build debug
```

Artefacts land in `out/Linux-aarch64/bin/<Config>/` and are owned by your
host user. Both X11 and Wayland backends are built, same as the x86_64 host
build.

The image is defined in [`docker/aarch64.Dockerfile`](../docker/aarch64.Dockerfile);
its CMake toolchain file is
[`docker/aarch64-toolchain.cmake`](../docker/aarch64-toolchain.cmake).
Deploying and running those artefacts on a physical device is outside the
repo's scope — wrap the `docker run` above in whatever personal script you
prefer.

