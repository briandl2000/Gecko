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

# Run a program (writes JSON to bench_results/<run_name>/<program>.json)
gk bench run debug_renderer -o baseline

# Run again after a change
gk bench run debug_renderer -o my_change

# Diff two runs as an HTML report (chart.js, opens in any browser)
gk bench compare baseline my_change
# -> bench_results/_reports/baseline_vs_my_change.html
```

Defaults: Release config; default run name is `_latest` if `-o` is
omitted.

## Layout

```
bench/<program>/CMakeLists.txt       -> produces `bench_<program>` exe
bench/<program>/bench_<program>.cpp  -> bench cases (static registration)

include/gecko/bench/                 -> public harness API (Gecko::Bench)
src/bench/                           -> harness implementation +
                                        default main (Gecko::BenchMain)

scripts/commands/bench.py            -> `gk bench` command
scripts/bench_report.py              -> HTML report generator (stdlib only)

bench_results/<run_name>/            -> JSON results (gitignored)
bench_results/_reports/              -> generated HTML diffs (gitignored)
```

Bench binaries land in `out/<plat>/bin/Release/bench/bench_<program>`.

Results are intentionally **outside** `out/`, so they survive `gk clean`
and stay around when you switch branches or rebuild.

## Writing a bench case

Add `bench/<your_module>/CMakeLists.txt`:

```cmake
add_executable(bench_<your_module> bench_<your_module>.cpp)
target_link_libraries(bench_<your_module>
  PRIVATE Gecko::Bench Gecko::BenchMain)
```

Make sure `bench/CMakeLists.txt` has `add_subdirectory(<your_module>)`.

Then in `bench/<your_module>/bench_<your_module>.cpp`:

```cpp
#include <gecko/bench/bench.h>

static void my_case(::gecko::bench::State& s)
{
    // Setup runs once (NOT timed).
    MyContext ctx;
    ctx.Init();

    for (auto _ : s) {
        // Body runs `Iterations` times. Each iteration is timed.
        ctx.DoWork();
    }
    // Teardown runs once after the loop (NOT timed).
}

GECKO_BENCH(my_case)
    .Iterations(200)
    .Warmup(10)
    .Description("Short description shown in JSON output.");
```

`GECKO_BENCH(fn)` registers the function. The function name doubles
as the case name; override with `.Name("display_name")`.

### Timing sub-sections

Inside the loop, scope a sub-timer with `ScopedSection`:

```cpp
for (auto _ : s) {
    {
        ::gecko::bench::State::ScopedSection sec(s, "cpu_record");
        ctx.RecordWork();
    }
    {
        ::gecko::bench::State::ScopedSection sec(s, "gpu_submit");
        ctx.SubmitWork();
    }
}
```

Sections are aggregated (count + total + mean per section) and reported
alongside the per-iteration totals.

### Multi-axis sweeps

Run the same case across multiple parameter values with `.Sweep(...)`:

```cpp
static void my_sweep(::gecko::bench::State& s)
{
    const ::gecko::i64 n = s.Arg("n");
    for (auto _ : s) { /* work proportional to `n` */ }
}

GECKO_BENCH(my_sweep)
    .Sweep("n",    {100, 1000, 10000, 100000})
    .Sweep("mode", {0, 1});
```

Stacked `.Sweep(...)` calls produce a Cartesian product (the example
above runs 8 invocations: 4 values * 2 modes). Each invocation is a
separate entry in the output JSON with its `args` field set.

### Counters

Record per-case scalar metrics for the JSON (e.g. items processed,
bytes uploaded):

```cpp
s.Counter("triangles_per_frame", 12345);
```

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
+ DebugRenderer modules:

```cpp
#include <gecko/bench/graphics_fixture.h>

static void my_render_case(::gecko::bench::State& s)
{
    ::gecko::bench::GraphicsFixture fx({.Title = "bench/my",
                                        .Width = 1280,
                                        .Height = 720,
                                        .VSync = false});
    if (!fx.IsValid()) { s.Abort("graphics setup failed"); return; }

    auto* device = fx.Device();
    for (auto _ : s) {
        fx.PumpEvents();
        if (auto frame = fx.BeginFrame(); frame.Valid) {
            // ... record + execute cmd list ...
            fx.Present(frame);
        }
    }
}
```

Use `.Visible = false` to run headless once the underlying platform
windows backend supports it (today the X11/Wayland backends require a
display).

## CLI

```text
gk bench list [<program>]            list programs / cases
gk bench run <program> [<case>]      build + run; write JSON
  -o <run_name>                      output run name (default: _latest)
  --config <debug|release>           default: release
  --build-only                       build but don't execute
  --iters <n>                        override per-case iteration count
  --warmup <n>                       override per-case warmup count

gk bench compare <run_a> <run_b>     emit HTML diff
  -o <out.html>                      default: bench_results/_reports/<a>_vs_<b>.html
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

# Diff
gk bench compare main feature
xdg-open bench_results/_reports/main_vs_feature.html
```

The harness records `git`, `timestamp`, `platform`, and `build_config`
in the JSON `meta`, so even if you forget what a run was, the JSON
remembers.

## JSON schema

```jsonc
{
  "meta": {
    "program": "debug_renderer",
    "timestamp": "2026-05-02T23:26:10Z",
    "platform": "Linux",
    "build_config": "Release",
    "git": "d587fc3"
  },
  "cases": [
    {
      "name": "debug_lines",
      "description": "...",
      "args": { "n": 1000 },              // sweep values, if any
      "iterations": 100,
      "stats_ns": {
        "min": 2409928, "max": 4109457,
        "mean": 3044956, "p50": 2765378, "p95": 4109457,
        "stddev": 662142
      },
      "sections": {
        "cpu_record": { "count": 100, "total_ns": 9665982,
                        "mean_ns": 1933196 }
      },
      "counters": { "lines_per_frame": 128000 },
      "samples_ns": [2409928, 3502148, ...]
    }
  ]
}
```

## What's NOT in v1

- **Memory tracking**: `IAllocator` and `gecko::TrackingAllocator`
  expose live counters; sampling them per-iteration is straightforward
  but not yet wired in.
- **GPU timing**: `gecko::graphics::IGpuProfiler` is the right hook
  but the bench harness doesn't currently pull GPU sections into the
  JSON.
- **Statistical regression detection**: today the HTML report shows
  raw delta-percent. A "is this delta significant?" Welch's-t test is
  on the radar.
- **CI integration**: results stay local for now.
