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

## Code Formatting

Format all C++ files in the project:

**Command line:**
```bash
./scripts/format.sh
```

**VS Code Task (optional):**
Add this to your local `.vscode/tasks.json` (not synced with git):
```json
{
  "label": "Format All Files",
  "type": "shell",
  "command": "find src include examples -type f \\( -name '*.cpp' -o -name '*.h' \\) -exec clang-format -i {} \\;",
  "options": {
    "cwd": "${workspaceFolder}"
  },
  "problemMatcher": []
}
```

Run with: `Ctrl+Shift+P` → `Tasks: Run Task` → `Format All Files`


## Install / Package
Gecko exports CMake targets (e.g. `Gecko::Core`, `Gecko::Runtime`).

- Install (example):
  - `cmake --install build --config Release --prefix <install-dir>`

After install you should have a package config under:
- `<install-dir>/lib/cmake/Gecko/`

Notes:
- If you change exported target names/paths, keep the config + export names in sync.
