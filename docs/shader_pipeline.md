# Shader Pipeline (`gecko_add_shaders`)

A single CMake helper compiles HLSL shaders to SPIR-V and embeds them into a
target as a generated header. The header exposes each blob as
`inline constexpr unsigned char` arrays via C++26 `#embed`, so any consumer
just `#include`s the header and uses the bytes directly.

## Quick Start

```cmake
gecko_add_shaders(
  TARGET    my_app
  NAMESPACE my_app::shaders        # optional
  HEADER    shaders.h              # default: "shaders.h"
  SHADERS
    triangle.vert.hlsl             # auto var name → TriangleVert
    triangle.frag.hlsl=TriangleFrag # explicit override
    plasma.comp.hlsl=PlasmaComp
)
```

By default `gecko_add_shaders` looks for HLSL sources under
`${CMAKE_CURRENT_SOURCE_DIR}/shaders`. Override with `SOURCE_DIR <path>`.

In code:

```cpp
#include "shaders.h"

using namespace my_app::shaders;
ShaderDesc vsDesc {
    .Bytes = { reinterpret_cast<const ::gecko::byte*>(TriangleVert),
               sizeof(TriangleVert) },
};
```

## Stage Inference

Stage is derived from the second-to-last extension:

| Filename suffix | glslc stage |
|-----------------|-------------|
| `.vert.hlsl`    | vertex      |
| `.frag.hlsl`    | fragment    |
| `.comp.hlsl`    | compute     |
| `.geom.hlsl`    | geometry    |
| `.tesc.hlsl`    | tess. control |
| `.tese.hlsl`    | tess. eval  |
| `.mesh.hlsl`    | mesh        |
| `.task.hlsl`    | task        |

Files that don't follow this pattern raise a configure-time error. There is no
override syntax for the stage today; rename the file or add a stage entry to
`cmake/GeckoShaders.cmake`.

## Variable Naming

If no override is given, the default variable name is the basename with each
dot-separated segment capitalized:

| Filename             | Default var      |
|----------------------|------------------|
| `triangle.vert.hlsl` | `TriangleVert`   |
| `fullscreen.frag.hlsl` | `FullscreenFrag` |
| `plasma.comp.hlsl`   | `PlasmaComp`     |

Override per shader with `=<VarName>`:

```cmake
SHADERS
  pp/blur_h.frag.hlsl=BlurHorizontal
  pp/blur_v.frag.hlsl=BlurVertical
```

## glslc Discovery

`gecko_add_shaders` calls `find_program(GECKO_GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)`.
If `glslc` is unavailable the call prints a `STATUS` message and returns without
modifying the target — the caller is expected to either gate the target on
`GECKO_GLSLC` or accept that the embedded shader bytes will be missing. The
provided `examples/graphics_example` gates itself.

Install:

- Linux: `libvulkan-dev glslc` (apt) / `vulkan-devel spirv-tools` (pacman)
- MSYS2 UCRT64: `mingw-w64-ucrt-x86_64-{vulkan-headers,vulkan-loader,shaderc}`

## What It Wires Up

- One `add_custom_command` per shader producing `${basename}.spv`.
- One `${TARGET}_shaders` custom target depending on every `.spv`.
- `add_dependencies(${TARGET} ${TARGET}_shaders)` so glslc runs before compile.
- `target_include_directories(${TARGET} PRIVATE …)` for the generated header.
- `target_compile_options(${TARGET} PRIVATE --embed-dir=…)` (GCC/Clang only).

## Limitations

- HLSL only (passes `-x hlsl` to glslc).
- One entry point: `main`.
- No permutation / `#define` matrix support — separate ticket.
- No runtime shader loading — the helper is purely build-time embedding.
