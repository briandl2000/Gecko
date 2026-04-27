# Profiling Guide

How to instrument Gecko code with the profiler v2 API, and how to read
the resulting traces in Perfetto / `chrome://tracing`.

All profiler macros compile to `(void)0` when `GECKO_PROFILING` is not
defined, so leaving instrumentation in shipped code costs zero bytes
and zero cycles.

## Levels

Three runtime levels gate emission:

| Level | Use for |
|---|---|
| `Always` | Frame markers, top-of-loop drivers, queue submits, presents — anything you want visible in *every* build, including release. Also feeds `GetStats` for HUDs. |
| `Normal` | Standard CPU/GPU instrumentation kept during dev profiling sessions. Default for `_NORMAL_*` macros. |
| `Detailed` | Hot inner-loop instrumentation that lives in the source tree but is suppressed unless you ask for it (per-draw, per-particle, per-token). |

Set the runtime cap with `IProfiler::SetMinLevel(level)`. Events at a
level *more verbose* than the cap are dropped at the source.

There is also a compile-time cap, `GECKO_PROF_MAX_LEVEL` (set per CMake
config). Debug builds default to `Detailed`, Release to `Normal`.

## Macros

### CPU scopes

Two families:

- **`GECKO_SCOPE_*`** — pushes a logging Label scope **and** opens a
  profiler zone. Use this when the code inside the block also logs and
  you want log lines prefixed with the same Label.
- **`GECKO_PROFILE_*`** — profiler zone only. Use everywhere else.

| Macro | Level |
|---|---|
| `GECKO_SCOPE(label)` / `_NAMED(label, name)` / `_CAT(label, name, cat)` | Detailed |
| `GECKO_SCOPE_NORMAL(label)` / `_NORMAL_NAMED` / `_NORMAL_CAT` | Normal |
| `GECKO_SCOPE_ALWAYS(label)` / `_ALWAYS_NAMED` / `_ALWAYS_CAT` | Always |
| `GECKO_PROFILE(label)` / `_NAMED` / `_CAT` | Detailed |
| `GECKO_PROFILE_NORMAL(label)` / `_NORMAL_NAMED` / `_NORMAL_CAT` | Normal |
| `GECKO_PROFILE_ALWAYS(label)` / `_ALWAYS_NAMED` / `_ALWAYS_CAT` | Always |

The plain (no-name) form uses the function symbol as the zone name.

### GPU scopes

The graphics module exposes `IGpuSampler`. Each `BeginZone` / `EndZone`
records a pair of timestamps on the queue; results are resolved one
frame later and emitted as `ProfEvent`s with `Source::GPU` on a
synthetic thread row named `"GPU"`.

Wire a sampler to a command list once per frame:

```cpp
sampler->BeginFrame(*cmd);
cmd->AttachGpuSampler(sampler.get(), labels::Renderer);
// ... record draws / dispatches ...
sampler->EndFrame(*cmd);
```

`AttachGpuSampler` does two things:

1. Enables **automatic** GPU zones around every `Draw*` and `Dispatch*`
   on this command list, tagged at level `Detailed`.
2. Lets the `GECKO_GPU_SCOPE_*` macros find the sampler implicitly.

Manual zones for higher-level groups (passes, post-process, GBuffer):

| Macro | Level |
|---|---|
| `GECKO_GPU_SCOPE_NAMED(cmd, label, name)` | Detailed |
| `GECKO_GPU_SCOPE_NORMAL_NAMED(cmd, label, name)` | Normal |
| `GECKO_GPU_SCOPE_ALWAYS_NAMED(cmd, label, name)` | Always |
| `GECKO_GPU_PROF_SCOPE(sampler, cmd, label, name)` | Normal (legacy explicit-sampler form) |

Constraints:

- Up to `GpuSamplerDesc::MaxZonesPerFrame` zones per frame (default 64).
  Each zone consumes 2 timestamps.
- Nesting up to 32 deep.
- A zone's timestamps are only visible to the profiler one frame later
  (after the GPU fence signals). No CPU stall.
- The current sampler is bound to the **graphics queue's** frame fence
  — don't attach it to compute command lists submitted on a separate
  queue (timestamps will never resolve and you will hang).

### Frame markers and counters

```cpp
GECKO_FRAME(labels::App, "Frame");        // resets aggregator stats
GECKO_COUNTER(labels::Memory, "LiveBytes",
              tracker.TotalLiveBytes());  // time-series counter row
```

## What to instrument

| Where | Level |
|---|---|
| Application main loop (`AppRun`, `Frame`, `RenderFrame`) | Always |
| `BeginFrame` / `Present` / `vkQueueSubmit` / `vkQueuePresentKHR` | Always |
| Job submit, job wait, mutex contention edges | Normal |
| Per-pass / per-dispatch CPU recording | Normal |
| Asset loaders, mesh / texture upload | Normal |
| `Draw` / `Dispatch` (GPU side) | Detailed (auto via `AttachGpuSampler`) |
| Per-element loops (per-particle, per-token) | Detailed |
| Logger / trace-sink / I/O writes | Detailed |

## Trace sink: `AsyncTraceProfilerSink`

```cpp
runtime::AsyncTraceProfilerSink traceSink("gecko_trace.json");
auto* p = GetProfiler();
traceSink.RegisterWith(p);
p->SetTraceEnabled(true);                 // arm the sink
traceSink.SetMinLevel(p->GetMinLevel());  // optional: cap trace verbosity
```

The sink runs on its own worker thread, double-buffers events, and
fsyncs every ~100 ms. The destructor drains the queue and closes the
JSON cleanly.

### Reducing trace size — `SetMinLevel` on the sink

`traceSink.SetMinLevel(ProfLevel::Normal)` keeps the profiler itself
fully detailed (so HUD / `WatchScope` aggregates remain accurate) but
drops `Detailed` zones at the sink boundary, before they hit disk.
ZoneBegin and ZoneEnd are paired by Level so the JSON stays well-formed.
Counter and Mark events bypass the filter.

Typical pattern in an example app:

```cpp
::gecko::ProfLevel traceLevel = profiler->GetMinLevel();   // match profiler
if (const char* env = ::std::getenv("APP_TRACE_LEVEL")) {
  if (env[0] == 'a')      traceLevel = ProfLevel::Always;
  else if (env[0] == 'd') traceLevel = ProfLevel::Detailed;
  else                    traceLevel = ProfLevel::Normal;
}
traceSink.SetMinLevel(traceLevel);
```

In debug builds this gives you everything; in release it auto-thins to
`Normal`; an env var lets you override on the fly without rebuilding.

### Other sinks

| Sink | When |
|---|---|
| `AsyncTraceProfilerSink` | Profiling sessions with a clean shutdown. Smallest overhead. |
| `CrashSafeTraceProfilerSink` | Shipping / crash-debug. JSON stays valid even on abnormal exit. Synchronous writes. |
| `TraceFileSink` | Legacy synchronous sink. Fine for short demos. |

## HUD stats: `WatchScope` + `GetStats`

```cpp
prof->WatchScope("Frame",        240);
prof->WatchScope("RenderFrame",  240);
prof->WatchScope("TrianglePass", 240, ProfSource::GPU);

auto s = prof->GetStats("Frame");
// s.LastNs, s.MinNs, s.MaxNs, s.AvgNs (rolling), s.Count
```

`WatchScope(name, window, source)` opts a single scope into a
fixed-size rolling window so HUDs can read live values without touching
the trace file. Without `WatchScope`, only `_ALWAYS_*` zones populate
`GetStats`.

## Categories

Categories are u8 ids (1..63) used to filter classes of scopes at
runtime:

```cpp
const u8 catTerrain = GetProfiler()->RegisterCategory("terrain");
GECKO_PROFILE_CAT(labels::World, "Terrain::Bake", catTerrain);
```

The category surfaces in the trace as a separate Perfetto category, so
you can mute / solo whole feature areas in the viewer.

## Diagnostics

```cpp
auto d = GetProfiler()->GetDiagnostics();
// d.DroppedEvents       — ring overflow (bump RingProfiler capacity)
// d.ReentrantDrops      — events emitted from inside the profiler itself
// d.AggregatorOverflow  — > 1024 distinct _ALWAYS names this frame
```

Non-zero `DroppedEvents` means the event rate exceeds the ring buffer.
Bump `RingProfilerDesc::Capacity` (must be a power of two).

## Patterns

### Wrap a custom GPU pass

```cpp
{
  GECKO_GPU_SCOPE_NORMAL_NAMED(*cmd, labels::Renderer, "PostProcess");
  cmd->BindPipeline(tonemapPipeline);
  cmd->Draw(3);   // automatically nests an inner "Draw" zone (Detailed)
}
```

The outer `PostProcess` is `Normal` so it survives a `Normal`-capped
trace. The inner per-draw zones are auto-emitted at `Detailed` and only
appear when you crank the trace level up.

### Wrap a CPU code block that also logs

```cpp
{
  GECKO_SCOPE_NAMED(labels::Renderer, "BuildVisibleSet");
  GECKO_INFO(labels::Renderer, "culling %u objects", n);
  cull(...);
}
```

The Label is pushed for the logger so `GECKO_INFO` formats with the
right prefix, and a profiler zone is opened for the duration.

### Hot inner loop

```cpp
for (const auto& d : draws) {
  GECKO_PROFILE(labels::Renderer);   // implicit name = function symbol
  cmd->Draw(d);
}
```

Compiles to nothing in release where `GECKO_PROF_MAX_LEVEL == Normal`.

## Currently-instrumented engine paths (snapshot)

| Module | Where | Level |
|---|---|---|
| Runtime | `EventBus::Subscribe` / `Send` / `Dispatch` / `Init` | Normal |
| Runtime | `JobSystem::Submit` / `Wait` / `WaitAll` / `WorkerIdle` | Normal / Detailed |
| Runtime | `RingLogger::Flush` | Normal |
| Runtime | `FileLogSink::Write` / `Flush` | Detailed |
| Runtime | `AsyncTraceProfilerSink::DrainAndWrite` / `Fsync` | Detailed |
| Platform | `PumpEvents` | Normal |
| Platform | Backend `Init` / `Shutdown` paths | Normal |
| Platform | Wayland callback dispatch | Detailed |
| Graphics | `VulkanDevice::Present` / `ExecuteGraphicsCommandList` / `vkQueueSubmit` / `vkQueuePresentKHR` | Always |
| Graphics | `VulkanDevice::BeginFrame` | Normal |
| Graphics | `VulkanCommandList::Begin` / `End` | Normal |
| Graphics | `VulkanCommandList::BeginRendering` / `EndRendering` | Detailed |
| Graphics | Auto GPU zones around `Draw*` / `Dispatch*` | Detailed |

When you add new code, follow the table: entry points and sleep/wait
edges as `Always` or `Normal`; deep inner work as `Detailed`.
