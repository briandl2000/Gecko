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

1. `TrackingAllocator` is installed via `AllocatorScope` (allocator is
   *infrastructure* — must be available before anything else).
2. Module objects (`RuntimeModule`, `PlatformModule`, app module) are
   constructed. Each module's default ctor owns sensible production
   defaults internally; pass a `Backends` struct only when you want to
   override a specific slot (custom logger, mock job system, ...).
3. `Engine::Create({...})` boots all modules in dependency order.
4. `StandardLogSinks` is engaged — it registers a `ConsoleLogSink` and
   a `FileLogSink` with the (now-real) logger and unregisters them on
   destruction.
5. The main loop runs windowed (`PumpEvents` + `DispatchEvents`) or
   headless work runs once.
6. Shutdown is RAII via the `App` destructor — members destruct in
   reverse declaration order: sinks unregister, then `Engine` tears
   modules down in reverse boot order, then `AllocatorScope` resets the
   allocator.

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
