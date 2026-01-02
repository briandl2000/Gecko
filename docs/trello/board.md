# Trello Board Template (Copy/Paste)

Tip: In Trello, you can create a List, then paste multiple lines to create multiple cards.

## List: Backlog
- Decide platform targets (Win/Linux first)
- Define engine “frame” lifecycle API
- Define handle + lifetime conventions for resources

## List: Platform (Windowing/Input)
- PlatformContext: window create/destroy API
- PlatformContext: event pump + dispatch
- Input: keyboard state + events
- Input: mouse position/buttons/wheel
- Window: resize + DPI awareness
- Window: cursor control (show/hide/lock)

## List: Graphics Abstraction
- Graphics API: adapter/device selection
- Graphics API: swapchain interface
- Graphics API: command queue + command list
- Graphics API: buffer creation + mapping
- Graphics API: texture creation + views
- Graphics API: shader module + pipeline creation
- Graphics API: render pass / dynamic rendering model decision
- Graphics API: descriptor/bind group model

## List: Vulkan Backend
- Vulkan: instance + debug messenger
- Vulkan: physical device selection
- Vulkan: logical device + queues
- Vulkan: surface + swapchain (from platform)
- Vulkan: command pool/buffer management
- Vulkan: synchronization primitives
- Vulkan: resource allocator strategy (VMA vs custom)
- Vulkan: pipeline cache + shader compilation workflow

## List: UI
- UI decision: ImGui vs custom
- UI: input integration layer
- UI: rendering integration (draw data to GPU)
- UI: docking + debug panels (optional later)

## List: ECS
- ECS: entity ID + generation
- ECS: component storage (SoA)
- ECS: queries/views
- ECS: system scheduler (jobs)
- ECS: transform component + hierarchy (later)

## List: Tooling/Quality
- Add sanitizer presets (ASAN/UBSAN/TSAN)
- Add minimal unit test target
- Add CI workflow (build Debug/Release)
- Add clang-format/clang-tidy baseline
- Add performance microbench harness

## List: Docs
- Write Services/Boot guide
- Write Logging guide (sinks, threading)
- Write Profiling guide (trace + counters)
- Write Memory guide (allocator contract + wrappers)
- Write Platform windowing guide
- Write Rendering abstraction guide
- Write Vulkan backend guide
- Write UI guide
- Write ECS guide
