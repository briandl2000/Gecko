# Benchmarking

Gecko ships its own minimal benchmark harness (`gecko::bench`) for
performance and regression tracking. It's deliberately small: each
benchmark is a normal C++23 executable that links the harness, registers
cases at static-init time, and is driven by `gk bench` from the command
line.

## TL;DR

```bash
# List benchmark programs in the repo
gk bench list

# List cases registered in a program
gk bench list debug_renderer

# Run a program (writes JSON to bench_results/<program>/<run>.json)
gk bench run debug_renderer -o baseline

# Run again after a change
gk bench run debug_renderer -o my_change

# View ALL runs of a program as one HTML report (auto-opens in browser)
gk bench results debug_renderer
```

Defaults: Release config; default run name is `_latest` if `-o` is
omitted.

## Layout

```
bench/<program>/CMakeLists.txt       -> produces `bench_<program>` exe
bench/<program>/test_*.cpp           -> bench cases (static registration);
                                        all `test_*.cpp` files glob into
                                        a single executable

include/gecko/bench/                 -> public harness API (Gecko::Bench)
src/bench/                           -> harness implementation +
                                        default main (Gecko::BenchMain)

scripts/commands/bench.py            -> `gk bench` command
scripts/bench_report.py              -> HTML report generator (stdlib only)

bench_results/<program>/<run>.json   -> JSON results (gitignored)
```

Bench binaries land in `out/<plat>/bin/Release/bench/bench_<program>`.

Results are intentionally **outside** `out/`, so they survive `gk clean`
and stay around when you switch branches or rebuild.

## Writing a bench case

Add `bench/<your_module>/CMakeLists.txt`:

```cmake
file(GLOB CONFIGURE_DEPENDS sources CONFIGURE_DEPENDS test_*.cpp)
add_executable(bench_<your_module> ${sources})
target_link_libraries(bench_<your_module>
  PRIVATE Gecko::Bench Gecko::BenchMain)
```

Make sure `bench/CMakeLists.txt` has `add_subdirectory(<your_module>)`.

Then drop one or more `test_<feature>.cpp` files into
`bench/<your_module>/`:

```cpp
#include <gecko/bench/bench.h>
#include <gecko/core/labels.h>
#include <gecko/core/utility/random.h>

constexpr ::gecko::Label kLabel = ::gecko::MakeLabel("bench.my_module");

static void my_case(::gecko::bench::State& s)
{
    // Setup runs once (NOT timed).
    MyContext ctx;
    ctx.Init();

    for (auto _ : s) {
        // Re-seed every iteration so the body is reproducible.
        ::gecko::SeedRandom(0xC0FFEEULL);

        // Body runs `Iterations` times. Each iteration is timed.
        GECKO_PROFILE_NORMAL_NAMED(kLabel, "do_work");
        ctx.DoWork();
    }
    // Teardown runs once after the loop.
}

GECKO_BENCH(my_case)
    .Iterations(60)
    .Warmup(100)
    .MetricLabel(kLabel)
    .Description("Short description shown in the report.");
```

`GECKO_BENCH(fn)` registers the function. The function name doubles
as the case name; override with `.Name("display_name")`.

**Three knobs you'll use on every case:**

| Builder method  | What it does |
|-----------------|---|
| `.Iterations(n)` | Number of *measured* iterations. |
| `.Warmup(n)`     | Spin-up iterations run *before* measurement (stabilises caches, JIT, GPU clocks). The CLI's `--warmup` only ever raises this floor; pass `--warmup 0` explicitly to force it down for fast smoke runs. |
| `.MetricLabel(label)` | Filter the captured profiler stream to only zones with this label. Drops engine-internal noise (Vulkan present, runtime, ...) from the report. Strongly recommended. |

For reproducibility, call `::gecko::SeedRandom(seed)` at the **top of
the body**. The bench is meant to compare runs of slightly different
code; if the input is randomised you're measuring variance, not
impact.

### Timing sub-sections

Annotate any code with the engine's existing profile macros
(`GECKO_PROFILE_NORMAL_NAMED`, `GECKO_PROFILE_NAMED`, etc.). The bench
harness installs an `IProfilerSink` for the duration of each
invocation: every CPU and GPU zone whose `BeginNs` falls inside one of
the measured iteration windows is binned into that iteration's stats
under its zone name. No bench-specific scope macro is needed.

```cpp
for (auto _ : s) {
    {
        GECKO_PROFILE_NORMAL_NAMED(kLabel, "cpu_record");
        ctx.RecordWork();
    }
    GECKO_GPU_PROF_SCOPE(*sampler, *cmd, kLabel, "gpu_draw");
    cmd->Draw(...);
}
```

The harness raises the profiler `MinLevel` to `Detailed` and sets
`DetailedSampleRate(1)` for the run, then restores both afterwards.
Every metric (frame total, CPU sub-zones, GPU zones) appears in the
JSON output as a separate entry under `metrics`.

### Built-in metrics

Every case automatically gets these two metrics, computed from the
per-iteration monotonic timestamps -- you don't have to do anything:

| Metric | Source | Unit | Meaning |
|---|---|---|---|
| `frame_total`      | cpu | ns | Wall-clock duration of the iteration body. |
| `iters_per_second` | cpu | hz | `1e9 / frame_total` -- real engine-side throughput, not a JS derivation. |

`iters_per_second` is the right number to look at for a render bench:
it bounds how often you can run the work, and it scales monotonically
with the GPU/CPU cost of the iteration.

### Custom metrics (non-time)

For anything that isn't time -- counts, ratios, your own throughput
numbers -- record one value per measured iteration with `State::Record`:

```cpp
for (auto _ : s) {
    const auto stats = ctx.DoWork();
    s.Record("draw_calls",    double(stats.DrawCalls), Unit::Count);
    s.Record("verts_per_sec", double(stats.Verts) / dt, Unit::Hz);
}
```

The three units (`Unit::Ns`, `Unit::Hz`, `Unit::Count`) drive axis
labels and unit auto-picking in the report. Calls during warmup are
ignored. Custom metrics show up in the report tagged `user` (green).

### Multi-axis sweeps

Run the same case across multiple parameter values with `.Sweep(...)`:

```cpp
static void my_sweep(::gecko::bench::State& s)
{
    const ::gecko::i64 n = s.Arg("n");
    for (auto _ : s) { /* work proportional to `n` */ }
}

GECKO_BENCH(my_sweep)
    .Sweep("n", {500, 1000, 2000, 4000, 8000})
    .MetricLabel(kLabel);
```

Stacked `.Sweep(...)` calls produce a Cartesian product. Each
invocation is a separate entry in the output JSON with its `args`
field set.

## Report layout

For each case, one big chart and a `metric:` dropdown above it:

* **No sweep**: grouped bar chart. X = run name, three bars per run
  for `min`, `mean`, `max`. Compare runs side-by-side at a glance.
* **One-axis sweep**: line chart. X = sweep value, one line per run
  showing the **mean**. Below the chart, a slider scrubs to a single
  sweep value and shows the same min/mean/max bar layout for that
  pinned value.
* **Multi-axis sweep**: bar chart with composite `[axis=v, ...]` X
  labels.

The report is a single self-contained HTML file (Chart.js loaded from
a CDN; works offline once cached).

### Aborting

If setup fails, abort cleanly:

```cpp
if (!ctx.IsValid()) {
    s.Abort("ctx setup failed");
    return;
}
```

### Graphics benches

For benches that need a window + graphics device, the harness ships a
small `GraphicsFixture` helper that boots Runtime + Platform + Graphics
+ DebugRenderer modules and exposes the per-frame `IGpuSampler`:

```cpp
#include <gecko/bench/graphics_fixture.h>

static void my_render_case(::gecko::bench::State& s)
{
    ::gecko::bench::GraphicsFixture fx({.Title = "bench/my",
                                        .Width = 1280,
                                        .Height = 720,
                                        .VSync = false});
    if (!fx.IsValid()) { s.Abort("graphics setup failed"); return; }

    auto* device  = fx.Device();
    auto* sampler = fx.GpuSampler();
    for (auto _ : s) {
        fx.PumpEvents();
        if (auto frame = fx.BeginFrame(); frame.Valid) {
            sampler->BeginFrame(*frame.CmdList);
            {
                GECKO_GPU_PROF_SCOPE(*sampler, *frame.CmdList,
                                     kLabel, "gpu_draw");
                // ... record draws ...
            }
            sampler->EndFrame(*frame.CmdList);
            fx.Present(frame);
        }
    }
}
```

GPU samples are resolved via the `IGpuSampler`'s frames-in-flight
queue, so the final ~3 iterations of a measured run will not have
GPU values. The harness tracks per-iteration *presence* and emits
those slots as `null` in `samples`; stats are computed only from
the present samples (no zero-fill, no min=0 artefact).

## Run artefacts

Each `gk bench run` writes the JSON results plus one snapshot per
requested working-dir file into `bench_results/<program>/`:

| File                       | Contents |
|----------------------------|---|
| `<run>.json`               | Stats + samples per case (consumed by the report). |
| `<run>.<snapshot>`         | Tail of `working_dir/<snapshot>` produced by the run. |

Default snapshot is `log.txt` (so you get `<run>.log`). Use
`--snapshot FILE` to change or extend the list (it's repeatable);
`--snapshot ""` disables snapshots entirely. The `.txt` extension is
stripped from the snapshot name -- so `log.txt` lands as `<run>.log`,
but `gecko_trace.json` keeps its full name as `<run>.gecko_trace.json`.
Files with a compound suffix are skipped by `bench results` (only
single-suffix `.json` files are loaded as bench results).

The harness writes JSON only to disk when `--out` is given; running
the binary directly with no `--out` prints JSON to stdout for piping.

## CLI

```text
gk bench list [<program>]            list programs / cases

gk bench run <program> [<case>]      build + run; write JSON
  -o <run_name>                      output run name (default: _latest)
  --config <debug|release>           default: release
  --build-only                       build but don't execute
  --iters <n>                        override per-case iteration count
  --warmup <n>                       override per-case warmup count
  --snapshot <file>                  capture working_dir/<file> tail
                                     (repeatable; default: log.txt;
                                     pass "" to disable)

gk bench results <program>           collect every JSON in
                                     bench_results/<program>/ into one
                                     HTML report
  -o <out.html>                      save HTML to <out.html>
                                     (default: write to /tmp and open
                                     in the default browser)
```

Each bench binary also accepts a small CLI directly:

```text
bench_<program> [options]
  --case <name>     run only this case
  --list            list registered cases
  --out <path>      write JSON to <path>
  --iters <n>       override iterations
  --warmup <n>      override warmup
  --program <id>    program id recorded in JSON meta
  --git <hash>      git hash recorded in JSON meta
```

## Comparing across branches

Benchmark results live in `bench_results/`, outside `out/`. They're
gitignored, so they're per-developer. To compare two states of the
codebase:

```bash
# On feature branch
gk bench run debug_renderer -o feature

# Switch to baseline (e.g. main)
git switch main
gk bench run debug_renderer -o main

# View
gk bench results debug_renderer
```

The harness records `git`, `timestamp`, `platform`, and `build_config`
in the JSON `meta`, so even if you forget what a run was, the JSON
remembers.

## JSON schema

```jsonc
{
  "meta": {
    "program": "debug_renderer",
    "timestamp": "2026-05-03T08:00:00Z",
    "platform": "Linux",
    "build_config": "Release",
    "git": "abc1234"
  },
  "cases": [
    {
      "name": "debug_lines_dynamic",
      "description": "...",
      "args": {},                         // sweep values, if any
      "iterations": 120,
      "view": "bars",                     // or "lines" if swept
      "metrics": {
        "frame_total": {
          "source": "cpu",                // "cpu" | "gpu" | "user"
          "unit":   "ns",                 // "ns"  | "hz"  | "count"
          "stats": {
            "min": 2409928, "max": 4109457,
            "mean": 3044956, "p50": 2765378, "p95": 4109457,
            "stddev": 662142
          },
          "samples": [2409928, 3502148, ...]
        },
        "iters_per_second": { "source": "cpu",  "unit": "hz",    "stats": {...}, "samples": [...] },
        "cpu_record":       { "source": "cpu",  "unit": "ns",    "stats": {...}, "samples": [...] },
        "draw_calls":       { "source": "user", "unit": "count", "stats": {...}, "samples": [...] },
        "gpu_draw_lines": {
          "source": "gpu",
          "unit":   "ns",
          "stats":  {...},
          "samples": [..., null, null, null], // last few iters unresolved
          "sample_count": 27                  // present only when < iterations
        }
      }
    }
  ]
}
```

`frame_total` and `iters_per_second` are always present and are
computed by the harness from the per-iteration monotonic timestamps.
Profiler-derived metric names come straight from the `GECKO_PROFILE_*`
/ `GECKO_GPU_PROF_SCOPE` macros the case body uses; `user`-source
metrics come from `State::Record`.

`ns` samples are emitted as integers; `hz` and `count` samples as
floats. Stats are always floats.

### Report rendering

The report picks a display unit per chart from the geometric mean of
the values: `ns` -> `ns`/`us`/`ms`/`s`, `hz` -> `Hz`/`kHz`/`MHz`,
`count` -> `count`. Each chart has a `log` toggle button next to the
metric dropdown that switches the Y axis between linear and
logarithmic -- useful when sweep values span orders of magnitude
(e.g. 0.8 ms at 500 circles vs 5.3 ms at 8000).

## Workflow: using a bench during development

The harness is built around a "pin a baseline, then iterate" loop. The
typical session looks like:

1. **Pin a baseline before you start.** Get the current code into a
   state you trust (visually correct, tests pass), then capture it:

   ```bash
   gk bench run debug_renderer -o baseline
   ```

   `bench_results/<program>/` is gitignored, so if you want a
   long-lived baseline (e.g. the numbers a doc/PR refers to), copy
   it next to the bench source or into a `line_renderer/` style
   subfolder under `bench_results/<program>/`.

2. **Make your change.** Verify visually first -- a "faster" result
   that draws the wrong thing isn't faster (see the cautionary tale
   below).

3. **Run the change under the same name scheme:**

   ```bash
   gk bench run debug_renderer -o my_change
   gk bench results debug_renderer        # opens HTML report
   ```

   The report stacks every run side-by-side. Use the metric dropdown
   to flip between `frame_total`, `cmd_submit`, GPU timers, and your
   custom counters; toggle `log` for sweep cases that span orders of
   magnitude.

4. **Promote when it sticks.** If the win holds up across a couple of
   runs, overwrite the pinned baseline:

   ```bash
   gk bench run debug_renderer -o baseline
   ```

   If it doesn't hold up, revert the change and the pinned baseline
   is still there to compare the next attempt against.

### Reading the numbers (and noise)

Real measurements on a desktop OS are noisy. Trust **`mean`** and
**`p50`** for trend; treat **`max`** as worst-case-observed, not a
regression signal -- a single page fault, IRQ, or compositor wakeup
will spike the max without changing the underlying perf.

Rough expectations on a normal desktop:

| metric class                  | typical CV (stddev/mean) |
|-------------------------------|--------------------------|
| GPU timer scopes              | < 5 % (very tight)       |
| `frame_total`, `cmd_submit`   | 5 - 15 %                 |
| Small CPU scopes (~10s of µs) | 15 - 35 % (noise-bound)  |

Small CPU scopes look bad as a percentage but the absolute swing is
tens of microseconds -- a few cache misses or a scheduler hiccup. A
real regression shows up as a shift in **`mean`** *and* **`p50`**
together across multiple runs, not as a one-off `max` spike.

If you ever need tighter numbers (regression hunt, paper-quality
comparison):

- Pin the bench to a single core: `taskset -c 3 gk bench run ...`.
- Set the CPU governor to `performance` and disable turbo so the
  clock doesn't wander.
- Run on a TTY or with the compositor off so Wayland/X11 wakeups
  don't sneak in.
- Bump `--iters` so jitter averages out.

For day-to-day "did my change help?" the defaults are fine.

### Cautionary tale: always look at the picture

The bench will happily report that broken code is faster than correct
code. While developing the line renderer's `EndFrame` upload, the
"baseline" passed `slot.CPU.size()` (a `std::vector<Line2D>::size()`,
i.e. **elements**) as the byte count of an `UploadBufferData` span.
That made the "baseline" upload a fixed `lineCapacity` *bytes*
regardless of how many lines were recorded -- truncated geometry on
screen, but a smaller copy on the wire. The "fix" (`sizeof(Line2D) *
m_LineCursor`) uploaded the correct byte range and looked *slower* in
the bench at every non-trivial scene size.

The lesson: before comparing two runs, confirm both produce the same
visual output. The bench measures what you ran, not what you meant.

## What's NOT here

- **Memory tracking**: `IAllocator` and `gecko::TrackingAllocator`
  expose live counters; sampling them per-iteration is straightforward
  but not yet wired in.
- **Statistical regression detection**: the report shows raw stats
  side-by-side. A "is this delta significant?" Welch's-t test is on
  the radar.
- **CI integration**: results stay local.
