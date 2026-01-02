# Build & Install

## Requirements
- CMake 3.22+
- C++23 compiler (GCC 13+, Clang 16+, MSVC 2022)
- Ninja (recommended)

## Configure + Build
From repo root:

- Debug:
  - `cmake --preset debug`
  - `cmake --build build --config Debug`

- Release:
  - `cmake --preset release`
  - `cmake --build build --config Release`

## Examples
Examples are enabled by default:
- `core_example`
- `platform_example`

Output binaries land under `build/out/bin/<Config>/`.

## Install / Package
Gecko exports CMake targets (e.g. `gecko::Core`, `gecko::Runtime`).

- Install (example):
  - `cmake --install build --config Release --prefix <install-dir>`

After install you should have a package config under:
- `<install-dir>/lib/cmake/Gecko/`

Notes:
- If you change exported target names/paths, keep the config + export names in sync.
