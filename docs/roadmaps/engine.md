# Engine Roadmap (High-Level)

This is a practical sequence that keeps the project shippable while you build big features.

## 1) Platform: Windowing + Input (minimum viable)
- Create window
- Event pump
- Keyboard/mouse input
- Time + DPI + resize handling

## 2) Graphics Abstraction (API + lifetime model)
- Device/adapter selection
- Swapchain
- Command buffers/queues
- Resource creation: buffers, textures, samplers
- Pipeline abstraction (graphics/compute)

## 3) Vulkan Backend (first implementation)
- Vulkan instance/device
- Swapchain + surface integration (from platform window)
- Resource allocator strategy (VMA or custom later)
- Debug layers, validation, capture-friendly defaults

## 4) UI Layer
- Decide approach: immediate mode (e.g. Dear ImGui) vs retained
- Input integration
- Renderer integration

## 5) ECS (minimum)
- Entity + component storage
- Query iteration
- System scheduling (jobs)
- Frame lifecycle + world ownership

## Supporting work (continuous)
- Test harness + sanitizers
- Asset pipeline plan (even if not implemented yet)
- Sample app that exercises window + renderer + UI + ECS
