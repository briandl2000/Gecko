# app_skeleton

The minimal "starting point" Gecko application. Use this as the
template when you start a new project.

## Run

```bash
gk run app_skeleton debug -- --no-window      # headless
gk run app_skeleton debug                     # windowed (1280x720)
gk run app_skeleton debug -- --frames=120     # auto-exit after N frames
```

CLI flags: `--no-window`, `--frames=N`, `--title=TEXT`,
`--backend=auto|null|xlib`, `--help`.

## What it shows

The full Gecko boot sequence, in order:

1. `TrackingAllocator` is installed via `SetAllocator()` (allocator is
   *infrastructure* — must be available before anything else).
2. Concrete service implementations (`ThreadPoolJobSystem`,
   `RingProfiler`, `RingLogger`, `EventBus`) are constructed.
3. Module objects (`CoreServicesModule`, `PlatformModule`, app module)
   are constructed but not yet started.
4. `Engine::Create({...})` boots all modules in dependency order.
5. Log sinks (`ConsoleLogSink`, `FileLogSink`) are attached to the
   logger after services are installed.
6. The main loop runs windowed (`PumpEvents` + `DispatchEvents`) or
   headless work runs once.
7. Shutdown is RAII via the `App` destructor — sinks unregister, the
   engine tears modules down in reverse, then `ResetAllocator()` runs.

## Files

| File | Role |
|---|---|
| [src/App.h](src/App.h) | `App` class declaration; owns all services + modules. |
| [src/App.cpp](src/App.cpp) | Boot/shutdown logic, headless and windowed runners. |
| [src/main.cpp](src/main.cpp) | CLI argument parsing → constructs the `App`. |

## Where to look next

- More involved demos: [core_example](../core_example), [platform_example](../platform_example), [graphics_example](../graphics_example).
- Module overview: [docs/architecture.md](../../docs/architecture.md)
- Service pattern: [copilot_context/service_dependencies.md](../../copilot_context/service_dependencies.md)
