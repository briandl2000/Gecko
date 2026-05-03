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
those slots as `null` in `samples_ns`; stats are computed only from
the present samples (no zero-fill, no min=0 artefact).

## Run artefacts

Each `gk bench run` writes two files into
`bench_results/<program>/`:

| File          | Contents |
|---------------|---|
| `<run>.json`  | Stats + samples per case (consumed by the report). |
| `<run>.log`   | Snapshot of `working_dir/log.txt` from this run. Useful for catching warnings / errors that surface during a bench (e.g. buffer-overflow warnings) without having them clobbered by the next run. |

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
          "source": "cpu",                // "cpu" | "gpu"
          "stats_ns": {
            "min": 2409928, "max": 4109457,
            "mean": 3044956, "p50": 2765378, "p95": 4109457,
            "stddev": 662142
          },
          "samples_ns": [2409928, 3502148, ...]
        },
        "cpu_record":     { "source": "cpu", "stats_ns": {...}, "samples_ns": [...] },
        "gpu_draw_lines": {
          "source": "gpu",
          "stats_ns": {...},
          "samples_ns": [..., null, null, null], // last few iters unresolved
          "sample_count": 27                     // present only when < iterations
        }
      }
    }
  ]
}
```

`frame_total` is always present and is computed by the harness from
the per-iteration monotonic timestamps. All other metric names come
straight from the `GECKO_PROFILE_*` / `GECKO_GPU_PROF_SCOPE` macros
the case body uses.

## What's NOT here

- **Memory tracking**: `IAllocator` and `gecko::TrackingAllocator`
  expose live counters; sampling them per-iteration is straightforward
  but not yet wired in.
- **Statistical regression detection**: the report shows raw stats
  side-by-side. A "is this delta significant?" Welch's-t test is on
  the radar.
- **CI integration**: results stay local.
