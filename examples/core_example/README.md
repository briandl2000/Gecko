# core_example

A guided tour of the `gecko::core` and `gecko::runtime` APIs. Walks
through every foundational service the engine boots: allocator, jobs,
events, profiler, logger.

## Run

```bash
gk run core_example debug
```

The example writes:
- log output to stdout and `working_dir/log.txt`
- a Chrome-trace JSON to `working_dir/gecko_trace.json` (open in
  [Perfetto](https://ui.perfetto.dev) or `chrome://tracing`)

## What's covered

| Step | Demo                  | API surface                                        |
|------|-----------------------|----------------------------------------------------|
| 1    | Memory management     | `AllocBytes`/`DeallocBytes`, `TrackingAllocator::TotalLiveBytes`, per-label stats |
| 2    | Event system          | `SubscribeEvent` (Immediate vs Queued), `SendEvent`, `DispatchEvents`, cross-thread send |
| 3    | Threading utilities   | `ThisThreadId`, `HardwareThreadCount`, `HighResTimeNs`, `SleepMs`/`PreciseSleepNs`/`YieldThread` |
| 4    | Job system            | `SubmitJob` parallel + dependency chain, `WaitForJobs`, main-thread `ProcessJobs` |
| 5    | Log levels            | `GECKO_TRACE`/`DEBUG`/`INFO`/`WARN`/`ERROR`        |
| 6    | Profiler diagnostics  | `IProfiler::GetDiagnostics`                        |

The profiler is forced to `ProfLevel::Detailed` for both the ring and
the trace sink so every emitted event shows up in the trace JSON.

## Files

| File | Role |
|---|---|
| [src/App.h](src/App.h) / [src/App.cpp](src/App.cpp) | Owns services + sinks; boots/teardown; runs each demo. |
| [src/Demos.h](src/Demos.h) / [src/Demos.cpp](src/Demos.cpp) | The actual demo functions (memory, events, threading, jobs, logging, profiler). |
| [src/Labels.h](src/Labels.h) | Module/event labels and the example event type. |
| [src/main.cpp](src/main.cpp) | Entry point (constructs `App`, calls `Run`). |

## Where to look next

- Boot pattern with a window: [app_skeleton](../app_skeleton)
- Service architecture: [copilot_context/service_dependencies.md](../../copilot_context/service_dependencies.md)
- Profiling guide: [docs/profiling.md](../../docs/profiling.md)
