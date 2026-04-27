# Profiling Guide

How to instrument Gecko code with the profiler v2 API.

## Macros at a glance

All macros below are zero-cost (`(void)0`) in non-`GECKO_PROFILING` builds.

| Macro | Use for | Level |
|---|---|---|
| `GECKO_FUNC(label)` | Implicit-name CPU scope at function entry. | Normal |
| `GECKO_PROF_FUNC(label)` | Same, no `GECKO_PUSH_LABEL` (profiler-only). | Normal |
| `GECKO_SCOPE_NAMED(label, name)` | Named CPU scope inside a function. | Normal |
| `GECKO_PROF_SCOPE_NAMED(label, name)` | Same, profiler-only. | Normal |
| `GECKO_PROF_SCOPE_NAMED_DETAILED(label, name)` | Hot inner-loop scope, suppressed at `Normal`. | Detailed |
| `GECKO_PROF_SCOPE_NAMED_MARK(label, name)` | Always-on; appears in trace and the `GetStats` aggregator. | Always |
| `GECKO_PROF_SCOPE_NAMED_CAT(label, name, cat)` | Categorised CPU scope. | Normal |
| `GECKO_FRAME(label, name)` | Frame boundary. Resets aggregator stats. | Always |
| `GECKO_COUNTER(label, name, val)` | Numeric counter sample. | Normal |
| `GECKO_GPU_PROF_SCOPE(sampler, cmd, label, name)` | GPU timestamp pair (explicit sampler). | Normal |
| `GECKO_GPU_SCOPE_NAMED(cmd, label, name)` | GPU zone, sampler picked up from cmd via `AttachGpuSampler`. | Detailed |
| `GECKO_GPU_SCOPE_NORMAL_NAMED(cmd, label, name)` | Same, Normal level. | Normal |
| `GECKO_GPU_SCOPE_ALWAYS_NAMED(cmd, label, name)` | Same, Always level. | Always |

## Levels

Three levels gate emission via `IProfiler::SetMinLevel`:

- **Always** — a scope marked `_MARK` always emits and updates the
  `GetStats` aggregator (min / max / last / count). Reserve for end-of-frame
  buckets, top-of-loop drivers, and a small set of system-wide hot paths.
- **Normal** — default for scopes you want during a profiling session
  but not in shipping. Use `GECKO_FUNC` / `GECKO_SCOPE_NAMED`.
- **Detailed** — opt-in via `_DETAILED`. Suppressed by default so you
  can leave deep instrumentation in the source tree (per-draw, per-glyph,
  per-particle) without paying for it.

## Where to instrument

### Always do
- Your application's main loop (one `GECKO_FRAME` per frame).
- Module `Startup` / `Shutdown`.
- Any sleep, wait, or join (`WaitForJob`, `vkQueueWaitIdle`, `Flush`).
- Engine entry points the user actively calls each frame:
  `PumpEvents`, `DispatchEvents`, `BeginFrame` / `Present`,
  `ExecuteGraphicsCommandList`, `JobSystem::Submit`.

### Probably do
- Each render pass / each compute dispatch (CPU side).
- Asset loaders, mesh / texture upload paths.
- Job functions you submit yourself — instrument the lambda body.

### Use `_DETAILED`
- Per-element loops (per-particle, per-draw, per-token).
- Anything called >1000×/frame.

### Use `_MARK`
- The frame driver, the GPU submit call, the present call.
- Any handful of canonical scopes you want a real-time HUD to read
  via `IProfiler::GetStats(FNV1aLiteral("MyScope"))`.

### Don't bother
- Trivial accessors / simple math.
- Code path that runs once at startup and never again (use a regular
  `GECKO_INFO` if interesting).

## CPU scope examples

```cpp
// Function entry — implicit name = function symbol.
void Renderer::DrawFrame() noexcept {
  GECKO_FUNC(labels::Renderer);
  // ...
}

// Inner block.
{
  GECKO_SCOPE_NAMED(labels::Renderer, "BuildVisibleSet");
  cull(...);
}

// Hot inner loop — suppressed unless level == Detailed.
for (const auto& d : draws) {
  GECKO_PROF_SCOPE_NAMED_DETAILED(labels::Renderer, "RecordDraw");
  cmd->Draw(d);
}

// Always-on, also fills GetStats() so a HUD can show min/max/last/count.
{
  GECKO_PROF_SCOPE_NAMED_MARK(labels::Renderer, "Renderer::Frame");
  // ...
}

// Categories let you toggle classes of scopes at runtime.
const u8 cat = GetProfiler()->RegisterCategory("net");
GECKO_PROF_SCOPE_NAMED_CAT(labels::Net, "Net::Tick", cat);
```

## Frame markers

```cpp
while (running) {
  PumpEvents();
  Update();
  Render();
  GECKO_FRAME(labels::App, "Frame");
}
```

`GECKO_FRAME` does two things: emits a `FrameMark` event into the trace
(frame separators in the Chrome viewer) and resets the aggregator so
`GetStats` durations are per-frame.

## Counters

```cpp
GECKO_COUNTER(labels::Memory, "LiveBytes",
              tracker.TotalLiveBytes());
```

Counters appear as time-series rows in the trace.

## GPU scopes

The Graphics module exposes `IGpuSampler`, created from a
`GraphicsDevice`. Each `BeginZone` / `EndZone` records a pair of
timestamps on the queue; results are resolved one frame later and emitted
as `ProfEvent`s with `Source::GPU` on a synthetic thread row named "GPU".

```cpp
auto sampler = device->CreateGpuSampler({
    .MaxZonesPerFrame = 32,
    .FramesInFlight   = 3,
    .GpuThreadName    = "GPU",
});

// Per frame:
sampler->BeginFrame(*cmd);
{
  GECKO_GPU_PROF_SCOPE(*sampler, *cmd, labels::Renderer, "TrianglePass");
  cmd->Draw(3);
}
sampler->EndFrame(*cmd);
device->ExecuteGraphicsCommandList(::std::move(cmd));
```

Constraints:
- ≤ `MaxZonesPerFrame` zones per `BeginFrame`/`EndFrame` pair.
- Nesting up to 32 deep.
- Resolution waits on the GPU fence — appearance lags by `FramesInFlight - 1`.

## Sinks

Choose one and register it after `Engine::Create`:

| Sink | When |
|---|---|
| `runtime::AsyncTraceProfilerSink("path.json")` | Profiling sessions where you want a clean shutdown. Drains + fsyncs in destructor. |
| `runtime::CrashSafeTraceProfilerSink("path.json")` | Shipping / crash-debug builds. JSON stays valid even on abnormal exit. |
| `runtime::TraceFileSink("path.json")` | Legacy synchronous sink. Fine for short demos. |

```cpp
runtime::AsyncTraceProfilerSink traceSink("gecko_trace.json");
if (auto* p = GetProfiler())
  traceSink.RegisterWith(p);
```

Open the resulting `.json` in Chrome's `chrome://tracing` or in Perfetto.

## Reading aggregator stats at runtime

```cpp
const u32 hash = ::gecko::FNV1aLiteral("Renderer::Frame");
ScopeStats s = GetProfiler()->GetStats(hash);
GECKO_INFO(labels::Hud, "Frame ms: last=%.2f min=%.2f max=%.2f n=%u",
           s.LastNs / 1.0e6, s.MinNs / 1.0e6, s.MaxNs / 1.0e6, s.Count);
```

Only `_MARK` (Always-level) scopes update the aggregator. The
aggregator is reset by `GECKO_FRAME` so values are per-frame.

## Diagnostics

```cpp
auto d = GetProfiler()->GetDiagnostics();
// d.DroppedEvents       — ring overflow
// d.ReentrantDrops      — events emitted from inside the profiler
// d.AggregatorOverflow  — > 1024 distinct _MARK names this frame
```

Non-zero `DroppedEvents` means the ring is too small for your event
rate; bump `RingProfiler` capacity (must be a power of two).

## Currently instrumented (engine-side)

| Module | Function | Level |
|---|---|---|
| Runtime | `EventBus::Subscribe` / `Send` / `Dispatch` / `Init` | Normal |
| Runtime | `JobSystem::Submit` (both overloads) | Normal |
| Runtime | `RingLogger::Flush` | Normal |
| Platform | `PumpEvents` | Normal |
| Platform | Most backend `Init`/`Shutdown` paths | Normal |
| Graphics | `VulkanDevice::BeginFrame` / `Present` / `ExecuteGraphicsCommandList` | Always (`_MARK`) |
| Graphics | `VulkanCommandList::Begin` / `End` | Normal |
| Graphics | `VulkanCommandList::BeginRendering` / `EndRendering` / `Dispatch` | Detailed |
| Graphics | `IGpuSampler` zones | GPU |

When adding new code, follow the table above — entry points and
sleep/wait edges always; deep inner work as `_DETAILED`.
