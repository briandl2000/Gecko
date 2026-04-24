# Graphics Module

Gecko's graphics module wraps a modern GPU API (currently Vulkan 1.3 with
dynamic rendering) behind a compact, backend-agnostic front end.

| Header | Purpose |
|---|---|
| [`gecko/graphics/graphics_types.h`](../include/gecko/graphics/graphics_types.h) | POD descs + opaque GPU handles (`Swapchain`, `RenderTarget`, `Buffer`, `Pipeline`, …) |
| [`gecko/graphics/command_list.h`](../include/gecko/graphics/command_list.h) | `GraphicsCommandList` + `ComputeCommandList` |
| [`gecko/graphics/graphics_device.h`](../include/gecko/graphics/graphics_device.h) | `GraphicsDevice`, `FrameContext`, `CreateGraphicsDevice()` |

## Backends

```cpp
enum class GraphicsBackend : u8 { Null, Vulkan };
```

- **`Null`** — default, zero-dep no-op implementation. All objects are
  invalid and all draws do nothing. Used when the Vulkan SDK is not available
  at build time (see [docs/build.md](build.md#installing-the-vulkan-sdk)).
- **`Vulkan`** — Vulkan 1.3 + dynamic rendering + VMA. Validation layers are
  enabled automatically when `GraphicsDeviceDesc::Debug == true` *and* the
  Khronos validation layer is installed on the system.

```cpp
auto device = gecko::graphics::CreateGraphicsDevice({
    .Backend = GraphicsBackend::Vulkan,
    .Debug   = true,
    .AppName = "MyApp",
});
```

## Ownership

GPU handles (`Swapchain`, `RenderTarget`, `Buffer`, `Pipeline`, `Texture`) are
small value types holding a ref-counted deleter that references the owning
device. **The `GraphicsDevice` must outlive every GPU object it created.**
Drop order in your `main` should therefore be:

```
<pipelines/buffers/textures/render targets> → swapchain → device → window
```

## Frame Model

```
┌────────────────────────────────────────────────────────────────┐
│  FrameContext f = device->BeginFrame(swapchain);               │
│  auto cmd = device->CreateGraphicsCommandList();               │
│                                                                │
│  cmd->Begin();                                                 │
│    cmd->BeginRendering(f.BackBuffer, &clear);                  │
│    cmd->SetViewport(...);  cmd->SetScissor(...);               │
│    cmd->BindPipeline(pipeline);                                │
│    cmd->BindVertexBuffer(vb);                                  │
│    cmd->Draw(3);                                               │
│    cmd->EndRendering();                                        │
│  cmd->End();                                                   │
│                                                                │
│  device->ExecuteGraphicsCommandList(::std::move(cmd));         │
│  device->Present(f);                                           │
└────────────────────────────────────────────────────────────────┘
```

`BeginFrame` returns a `FrameContext` that carries the acquired image index,
sync slot, and a render-target handle pointing at the current back buffer.
Pass it (or a span of them, one per swapchain) to `Present`.

### Sync model

- `MaxFramesInFlight = 2` — semaphores and fences cycle through this many
  slots regardless of how many images the swapchain actually holds.
- `MaxSwapchainImages = 8` — per-image resources (image, view, command pool
  reuse) are sized to the maximum any driver is likely to hand us.

Decoupling the CPU frame slot from the driver-chosen image count is what keeps
`ResizeSwapchain` safe and robust across drivers.

## Multi-Swapchain / Multi-Window

A single command list can touch any number of swapchains — each draw target
is tracked implicitly via `BeginRendering(RenderTarget)`. At `Present` time
pass all the frame contexts you want to flip:

```cpp
FrameContext fA = device->BeginFrame(swapA);
FrameContext fB = device->BeginFrame(swapB);
auto cmd = device->CreateGraphicsCommandList();

cmd->Begin();
  cmd->BeginRendering(offscreenRT, &clear);   // offscreen
    /* ... */
  cmd->EndRendering();

  cmd->BeginRendering(fA.BackBuffer, &clear); // window A
    /* ... */
  cmd->EndRendering();

  cmd->BeginRendering(fB.BackBuffer, &clear); // window B
    /* ... */
  cmd->EndRendering();
cmd->End();

device->ExecuteGraphicsCommandList(::std::move(cmd));
device->Present(::std::array{fA, fB});
```

Mixing offscreen render targets and swapchain back buffers in the same
command list is supported — layout transitions are handled automatically.

## Shaders

Shaders are passed as `ShaderCode`:

```cpp
struct ShaderCode {
    ShaderFormat        Format;   // SPIRV (today), DXIL/MSL later
    ::std::span<const byte> Bytes;
};
```

Examples compile HLSL → SPIR-V via `glslc` (shaderc) as a CMake custom
command and embed the resulting `.spv` bytes at load time using `#embed`.

## Resizing

Resize is driven by the application — usually in response to a platform
window-resize event:

```cpp
device->ResizeSwapchain(swapchain);
```

This waits for device idle, re-queries surface capabilities, rebuilds the
swapchain and all per-image resources, and increments
`swapchain.ResizeEpoch`. Pipelines and offscreen render targets are
unaffected.

## Debugging

- `GraphicsDeviceDesc::Debug = true` turns on validation layers when they are
  installed. Errors and warnings are routed through Gecko's logger
  (`gecko.graphics` label) at `ERROR`/`WARN` level.
- Because the default logger is the threaded `RingLogger`, early-startup
  validation messages may appear late. For diagnosing backend issues prefer
  the `ImmediateLogger` — see `examples/graphics_example` for the pattern.
- Set `VK_LOADER_DEBUG=all` (Vulkan loader) and `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`
  from the environment to force validation without rebuilding.
