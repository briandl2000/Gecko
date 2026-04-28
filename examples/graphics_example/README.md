# graphics_example

Two-window Vulkan demo. Window A shows a spinning, time-tinted triangle
rendered into an offscreen RT and blitted back. Window B shows a
compute-shader plasma effect.

> Requires `glslc` (Vulkan SDK / shaderc). Without it the example is
> silently dropped from the build. See [docs/shader_pipeline.md](../../docs/shader_pipeline.md).

## Run

```bash
gk run graphics_example debug
```

| Env var | Effect |
|---|---|
| `GECKO_VK_VALIDATION=1` | Enable Vulkan validation layers (slow under gdb). |

| Key | Action |
|---|---|
| Esc | Quit |
| F4  | Dump profiler stats to log |

The HUD log line prints once per second:

```
HUD frame=0.28ms (3612 fps) cpu_render=0.27ms gpu_tri=0.002ms gpu_blit=0.011ms draws=3 alloc_live=2 KB frames=3381
```

## What's covered

- Two windows + two swapchains driven by one device
- Offscreen render target, sampled in a fullscreen blit
- Indirect draw with `VkDrawIndirectCommand` layout in a structured buffer
- Push constants on both graphics and compute pipelines
- Compute pipeline writing into a `RWTexture2D<float4>` storage texture
- Two compute command lists per frame to exercise the multi-cmd-list
  GPU-profiling path
- Timestamp query pool + `IGpuSampler` so per-pass GPU times feed the
  same `IProfiler` aggregator the CPU side uses
- Watched scopes (`frame`, `renderFrame`, `TrianglePass`,
  `BlitPass`, `PlasmaCompute`) with rolling averages

## Files

| File | Role |
|---|---|
| [src/App.h](src/App.h) / [src/App.cpp](src/App.cpp) | Owns services, device, all GPU resources, all pipelines, the main loop, and the per-frame recording (`RecordComputePass`, `RecordTrianglePass`, `RecordBlitPass`). |
| [src/main.cpp](src/main.cpp) | Constructs `App`, calls `Run`. |
| [shaders/](shaders/) | HLSL sources, compiled to SPIR-V by `gecko_add_shaders()`. |

## See also

- [docs/graphics.md](../../docs/graphics.md)
- [docs/shader_pipeline.md](../../docs/shader_pipeline.md)
- [docs/profiling.md](../../docs/profiling.md)
