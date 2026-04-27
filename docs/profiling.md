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

The graphics module exposes `IGpuSampler`. A sampler owns one VkQueryPool
ring (one slot per frame in flight) and emits matched begin/end
`ProfEvent`s on a synthetic thread row whose name comes from
`GpuSamplerDesc::GpuThreadName`. Use one sampler **per Vulkan queue**.
Gecko currently has only one queue (graphics — also services compute and
present), so most apps create exactly one sampler with
`GpuThreadName = "GPU.Graphics"`. When a dedicated compute or copy queue
is added later, create a second sampler (`"GPU.Compute"` /
`"GPU.Copy"`); each shows up as its own thread track in Perfetto.

Per-frame setup:

```cpp
GpuSamplerDesc desc {};
desc.MaxZonesPerFrame = 64;     // 2 timestamps per zone
desc.FramesInFlight   = 3;
desc.GpuThreadName    = "GPU.Graphics";
auto sampler = device->CreateGpuSampler(desc);

// every frame:
sampler->BeginFrame(*cmd);              // rotates ring + resolves N-frames-old slot
cmd->AttachGpuSampler(sampler.get(), labels::Renderer);
// ... record draws / dispatches / GECKO_GPU_SCOPE_* zones ...
sampler->EndFrame(*cmd);                // no-op (kept for API symmetry)
cmd->End();
device->ExecuteGraphicsCommandList(::std::move(cmd));
```

`AttachGpuSampler` does **three** things:

1. Opens an **`Always`-level `"CommandList"` zone** that wraps the entire
   command-buffer execution. Closed in `cmd->End()`. This gives every
   submitted cmd list a free GPU-time-per-cmd-buffer bracket — no
   manual setup. Opt out by simply not attaching a sampler.
2. Enables automatic GPU zones around every `Draw*` and `Dispatch*` on
   this command list, tagged at level `Detailed`. They show up nested
   inside any manual zones you've opened.
3. Lets the ergonomic `GECKO_GPU_SCOPE_*` macros find the sampler
   implicitly via `cmd.GetAttachedGpuSampler()`.

Manual zones for higher-level groups (passes, post-process, GBuffer):

| Macro | Level |
|---|---|
| `GECKO_GPU_SCOPE_NAMED(cmd, label, name)` | Detailed |
| `GECKO_GPU_SCOPE_NORMAL_NAMED(cmd, label, name)` | Normal |
| `GECKO_GPU_SCOPE_ALWAYS_NAMED(cmd, label, name)` | Always |
| `GECKO_GPU_PROF_SCOPE(sampler, cmd, label, name)` | Normal (legacy explicit-sampler form) |

Constraints:

- Up to `GpuSamplerDesc::MaxZonesPerFrame` zones per frame across **all
  cmd lists that share the sampler** (default 64). Each zone consumes 2
  timestamps. The auto `"CommandList"` zone counts toward this budget,
  as do auto Draw/Dispatch zones.
- Nesting up to 32 deep across the whole frame's cmd-list graph.
- A zone's timestamps are visible to the profiler `FramesInFlight - 1`
  frames later (after the GPU has signalled). No CPU stall.

#### Multi-command-list and multi-queue

A single sampler is safe to attach to **any number of command lists in
the same frame**, as long as they all submit to the same queue.
Examples submit a graphics cmd plus two compute cmds (currently all on
the graphics queue) — each gets its own `"CommandList"` Always zone
nested with whatever passes you record on that cmd. Three free `Always`
ranges show up side-by-side on the GPU track per frame.

When Gecko gains an actual async-compute queue, attach a *separate*
sampler to compute cmds: per-queue VkQueryPool rings can't share state,
and Perfetto draws each sampler's `GpuThreadName` as its own row, which
is what you want anyway.

#### How GPU-frame timing works (under the hood)

`IGpuSampler` keeps `FramesInFlight` query-pool slots in a ring.

- `BeginFrame` advances the ring index and, if the slot we wrap into is
  pending, calls `ResolveSlot` (read all timestamps + emit events +
  host-reset the pool via `VK_EXT_host_query_reset` /
  Vulkan 1.2 core).
- `BeginZone` / `EndZone` write timestamps into the *current* slot at
  recording time. Cmd lists submitted out of recording order (e.g. a
  compute cmd recorded after a graphics cmd's `BeginFrame` but
  submitted before it) interleave their timestamp writes; the resolver
  uses `min(valid ts)` from the slot as the frame anchor so a negative
  delta cannot underflow `u64`.
- `EndFrame` is a no-op kept for API symmetry. Rotation happens in
  `BeginFrame` so any `EndZone` calls fired **after** `EndFrame` (e.g.
  the auto `"CommandList"` zone closed by `cmd->End()`) still target
  the correct slot.
- The pool is reset from the host inside `ResolveSlot`, **not** from a
  cmd-buffer-recorded `vkCmdResetQueryPool`. Embedding the reset on a
  recorded cmd list races with cmd lists submitted later in record
  order but earlier in GPU-execution order.

#### Aligning the GPU timeline with CPU events

GPU timestamps come from a different clock than CPU `Profiler::NowNs()`.
The sampler rebases them so GPU events sit on the same monotonic
timeline as CPU events. The anchor is the first per-frame
`IGpuSampler::OnSubmit(cpuNowNs)` call — fired by `VulkanDevice` right
before each `vkQueueSubmit`. Concretely:

```text
gpuFrameStart = min(valid GPU ts in slot)            (ns from GPU clock)
cpuAnchor     = first OnSubmit(cpuNowNs) for slot    (ns from CPU clock)
emitTs(gpuTs) = cpuAnchor + (gpuTs - gpuFrameStart)
```

Result: **the first cmd list of a frame's `CommandList` GPU zone lines
up with its `vkQueueSubmit` to within ~1 µs.** Subsequent cmd lists
within the same frame can drift by tens of µs.

##### Why later cmd lists' GPU zones can appear *before* their CPU submit

This is real GPU pipelining, not a bug. A small worked example:

```text
CPU thread :  [record cmd0][submit0][record cmd1][submit1][record cmd2][submit2]
                    ^ 0us       ^ 280us               ^ 560us

GPU queue  :       [exec cmd0][exec cmd1][exec cmd2]
                    ^ 0.5us    ^ ~250us   ^ ~500us
```

`vkQueueSubmit` is fully asynchronous: it just enqueues. The GPU starts
executing cmd 0 immediately, and when it finishes, the queue picks up
cmd 1 *right away* — the driver/GPU does not wait for the CPU to call
`vkQueueSubmit` again, because cmd 1 was already enqueued. If the GPU
finishes cmd 0 faster than the CPU records and submits cmd 1, **the
GPU starts executing cmd 1 before the CPU's `vkQueueSubmit(cmd1)` call
completes.** In Perfetto this looks like the GPU zone for cmd 1 starts
~70 µs to the *left* of its CPU submit. That's an accurate picture of
the hardware.

We deliberately **do not** clamp GPU zones to start ≥ their own submit:
doing so would hide the GPU-pipelining advantage you're trying to
measure.

##### When the alignment really is wrong

- **Long runs**: CPU and GPU clocks drift relative to each other (a
  few ppm typically). The first cmd list per frame is re-anchored
  every frame so error never accumulates across frames; within a
  frame it's bounded by frame time × ppm and is normally < 1 µs.
- **No submits**: if no `OnSubmit` arrives before `ResolveSlot`, the
  sampler falls back to the CPU time sampled in `BeginFrame`. This
  only matters in degenerate cases (recorded zones never submitted).
- **Multi-queue (future)**: each queue has its own GPU clock domain;
  use one `IGpuSampler` per queue (with distinct `GpuThreadName` like
  `"GPU.Compute"`) so each gets its own anchor.

If you ever need sub-microsecond CPU↔GPU clock conversion across a
whole frame's worth of cmd lists, `VK_EXT_calibrated_timestamps` is the
hammer to reach for. Gecko does not use it today; the single-anchor
model above is sufficient for visualizing pass-level GPU work.

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
| Graphics | GPU auto `"CommandList"` zone (per `AttachGpuSampler`) | Always |
| Graphics | Auto GPU zones around `Draw*` / `Dispatch*` | Detailed |

When you add new code, follow the table: entry points and sleep/wait
edges as `Always` or `Normal`; deep inner work as `Detailed`.

## What the GPU thread looks like in Perfetto

A frame from `examples/graphics_example` (one graphics cmd list, two
compute cmd lists, all on the single graphics queue) renders on the
`GPU.Graphics` thread row as:

```
[CommandList ─────────────────────────────────────]   ← compute cmd 0, Always
   [PlasmaPass]                                       ← Normal
      [Dispatch]                                      ← auto Detailed

[CommandList ─────────────────────────────────────]   ← compute cmd 1, Always
   [PlasmaPass]
      [Dispatch]

[CommandList ─────────────────────────────────────]   ← graphics cmd, Always
   [TrianglePass]                                     ← Normal
      [DrawIndirect]                                  ← auto Detailed
   [BlitPass]                                         ← Normal
      [Draw]   [Draw]                                 ← auto Detailed (per blit)
```

Three side-by-side `CommandList` ranges per frame is the expected shape
when an app submits multiple cmd buffers — they're physically distinct
GPU executions on the queue. You opt out of any of them by simply not
attaching the sampler to that cmd list.
