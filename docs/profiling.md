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

## Workflow — using the profiler day-to-day

This section is the practical "how do I actually use this" guide. It
covers three roles:

1. **Engine dev** — adding instrumentation to a new module / feature.
2. **App dev** — turning the profiler on, picking a level, capturing a
   trace.
3. **Trace reader** — opening the result and finding bottlenecks.

### 1. Engine-side: what to add and where

When you write a new subsystem, add zones at three layers:

**Always layer** (visible in release):

- One zone per public entry point that *drives* a frame or queue.
  Examples: `IGraphicsDevice::Present`, `IJobSystem::WaitAll`,
  `EventBus::Dispatch`, the module's `Update`/`Tick`/`Render`.
- Anything that can block or sleep — wait edges, vkQueueSubmit,
  vkQueuePresentKHR, fence waits, mutex acquires you suspect can stall.
- Frame markers and present points.

```cpp
GECKO_PROFILE_ALWAYS(labels::Renderer);   // outer entry point
```

**Normal layer** (default in dev profiling sessions):

- Entry points of internal helpers — pass record functions, resource
  uploads, asset loaders, scene queries.
- Anywhere you'd want a default "where did the 8 ms go?" answer.

```cpp
GECKO_PROFILE_NORMAL_NAMED(labels::Renderer, "BuildCullList");
```

**Detailed layer** (kept in source, off by default):

- Per-element loops (per-draw, per-particle, per-token, per-entity).
- Tight inner functions that make the trace 10× larger when enabled.
- Stuff useful when chasing one specific stall, not for routine viewing.

```cpp
GECKO_PROFILE(labels::Renderer);          // implicit Detailed
```

**GPU**: in any code path that records draws/dispatches, accept an
optional `IGpuSampler*` and call `cmd->AttachGpuSampler(sampler, label)`
once per command list. Wrap pass-level ranges in `GECKO_GPU_SCOPE_NORMAL`;
individual draws/dispatches get auto-zoned.

**Hard rules**:

- Never zone in code that runs *during* a profiler emission (logger
  formatters called from a profile sink, etc.) — risks reentrancy.
- Never call `GECKO_PROFILE*` inside the allocator's `Allocate`/`Free`.
  Zones allocate a stack slot only, but anything you put in the body
  (including `printf`-into-name) does. Pre-format names.
- Stable string literals only for `name`. `const char*` is captured
  by pointer, not copied. A `std::string::c_str()` whose owner dies is a
  trace-corrupting use-after-free.

**Counters**: emit a counter (level always survives the filter) for
anything you want plotted as a line on the trace:

```cpp
GECKO_COUNTER(labels::Mem, "alloc_live", liveBytes);
GECKO_COUNTER(labels::Renderer, "draws", drawCount);
```

Counters are cheap, batched, and show up as `chrome://tracing` line
charts above the thread rows.

### 2. App-side: how to actually use it

#### Instrumenting your own app code

The profiler macros are public API — your gameplay / tools / app code
should use them too, with the same level discipline as the engine:

```cpp
void Game::Tick(f32 dt)
{
    GECKO_PROFILE_ALWAYS(labels::Game);          // visible in release

    {
        GECKO_PROFILE_NORMAL_NAMED(labels::Game, "AI");
        m_AI.Update(dt);
    }
    {
        GECKO_PROFILE_NORMAL_NAMED(labels::Game, "Physics");
        m_Physics.Step(dt);
        GECKO_COUNTER(labels::Game, "rigid_bodies", m_Physics.BodyCount());
    }
    {
        GECKO_PROFILE_NORMAL_NAMED(labels::Game, "Render");
        for (auto& e : m_Entities)
        {
            GECKO_PROFILE(labels::Game);          // implicit Detailed per entity
            e.Render(m_Cmd);
        }
    }
}
```

Quick rules for app code:

- **One `_ALWAYS`** at each top-level entry point you call from the
  frame loop (`Tick`, `Update`, `Render`, `Save`).
- **`_NORMAL_NAMED`** for sub-systems / passes / loaders.
- **Implicit Detailed** for per-entity / per-particle inner work.
- Use **`GECKO_COUNTER`** for anything you'd want as a graph: entity
  count, AI agents, queued downloads, draw calls, memory by category.
- Define your app's labels once (e.g. `app::game::labels::AI`) and reuse
  them — Labels are the trace's category column in Perfetto.

You don't need to install or boot anything; if the engine is up, the
profiler is up, and your zones flow through the same ring + sinks as the
engine's.

#### Just want timings in the HUD

The profiler is always installed. If you want the HUD strip
(`F4`-style stats dump, draws, alloc_live, etc.) just register a
`WatchScope` in your app boot:

```cpp
auto* profiler = GetProfiler();
profiler->WatchScope("frame");          // rolling avg over last N samples
profiler->WatchScope("renderFrame");
profiler->WatchScope("Present");
```

Then in your HUD pull `profiler->GetStats("frame")` and format
`LastNs`, `AvgNs`, `MinNs`, `MaxNs`.

Default min level is set by build config (`Detailed` in debug,
`Normal` in release). Override per app:

```cpp
profiler->SetMinLevel(ProfLevel::Normal);   // tighten the firehose
```

#### Want to capture a trace JSON

Install an `AsyncTraceProfilerSink` *before* the work you want to
capture and let it go out of scope (or call its destructor) *after*.

```cpp
runtime::AsyncTraceProfilerSink trace("frame123.json");
trace.SetMinLevel(ProfLevel::Detailed);   // or Normal for smaller files
if (trace.IsOpen())
{
    if (auto* p = GetProfiler())
    {
        trace.RegisterWith(p);
        p->SetTraceEnabled(true);          // off by default — tracing is opt-in
    }
}

// ... run your scenario ...

// trace dtor unregisters, drains the worker thread, fsyncs the file.
```

`core_example` does this unconditionally at Detailed level (debug *and*
release) so it's the canonical "always emit a full trace" reference.
Other examples keep tracing off by default — turn it on only for
captures.

#### Want to capture exactly one frame

Wrap a single-frame scope with `SetTraceEnabled(true)` /
`SetTraceEnabled(false)` around the frame loop iteration of interest.
The sink keeps streaming; the profiler just stops handing it events.

### 3. Reading the captured data

The output is Chrome-trace JSON. Three viewers, all free:

- **Perfetto UI** (recommended) — <https://ui.perfetto.dev>. Drag the
  `.json` onto the page. Modern, supports counters, search, flame charts,
  thread filtering, link-sharing.
- **chrome://tracing** in any Chromium browser. Older but works
  offline.
- **`speedscope`** for a flame-graph view of one thread —
  `npx speedscope frame123.json`.

#### What to look for, in order

1. **Find the `frame` zone on the main thread.** That's your wall-clock
   budget. If it's longer than your target (e.g. 16.6 ms for 60 Hz)
   you're missing frame.
2. **Compare main-thread frame to the `GPU.Graphics` row.** Are they
   the same length, or is one much shorter?
   - Main longer → CPU-bound. Look for big zones on the main thread.
   - GPU longer → GPU-bound. Look at the GPU `CommandList` ranges and
     drill into `TrianglePass`/`BlitPass`/whatever.
   - Both shorter than `frame` → you're vsync-bound or sleeping. Check
     for an `Always`-level wait zone.
3. **Look at the `vkQueueSubmit` zone vs the GPU `CommandList` it
   produced.** They should be nearly back-to-back for the *first* cmd
   list of the frame. Later cmd lists' GPU ranges may legitimately
   start *before* their CPU submit (the queue picks up pre-recorded
   work as soon as the previous cmd finishes). See
   [Aligning the GPU timeline with CPU events](#aligning-the-gpu-timeline-with-cpu-events).
4. **Counters strip at the top.** `draws`, `alloc_live`, anything else
   you've published. Spikes here often correlate with frame-time spikes
   directly below.
5. **Worker-thread rows.** Long flat sections = idle. Short scattered
   zones = a stall on the main thread waiting for the worker. Use the
   `JobSystem::Wait*` Always zones on main as a cross-reference.

#### Trace size budgeting

Rough numbers from `examples/graphics_example` at 1 kHz frame rate:

| Setting                               | Bytes / minute |
|---------------------------------------|----------------|
| `Always`-only                         | ~5 MB          |
| `Normal`                              | ~50 MB         |
| `Detailed`                            | ~500 MB        |

Use `Detailed` for short captures (seconds, not minutes). Perfetto
starts to chug above ~1 GB; Chromium tracing chokes earlier.

### Summary cheat-sheet

| Goal | Action |
|---|---|
| Ship release builds with always-on telemetry | Use `GECKO_PROFILE_ALWAYS`/`GECKO_SCOPE_ALWAYS`; default profiler level. |
| Profile during dev | Default level (Normal in release / Detailed in debug). |
| One-shot deep trace | `AsyncTraceProfilerSink` + `SetTraceEnabled(true)` around the scenario. |
| HUD numbers | `WatchScope` + `GetStats`; counters via `GECKO_COUNTER`. |
| Reading | Perfetto UI; cross-check `frame` vs `GPU.Graphics`; check counters strip. |
