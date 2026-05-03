# Bench Methodology

How to read Gecko's bench results without fooling yourself. Read this
before optimizing anything based on a bench number.

## Anatomy of a Frame

Every graphics bench iteration measures the same five things:

| Metric | Source | What it covers |
|---|---|---|
| `frame_total` | cpu | The whole iteration (record + submit + execute + present + implicit waits) |
| `cpu_record` | cpu | App-side CPU work: scene generation **+** the renderer's CPU path |
| `cmd_submit` | cpu | `vkQueueSubmit` and anything that blocks on the graphics queue |
| `cmd_execute` | cpu | `ExecuteGraphicsCommandList` overhead (usually negligible) |
| `gpu_draw_lines` | gpu | GPU timestamp around the actual draw |

`frame_total` is **not** a clean sum of the others -- it includes:
- The gap **before** the iteration started where the previous
  iteration's GPU work finished (frames-in-flight pacing).
- Implicit syncs (swapchain `vkAcquireNextImageKHR`, GPU profiler
  timestamp readback).
- Any time `cmd_submit` blocked because the queue was busy.

## Pitfall #1: scene cost vs renderer cost

`cpu_record` measures **everything that happens between
`ctx.NewFrame()` and `cmd->EndRendering()`**. That includes the bench
generating its own scene -- random numbers, trig, input transforms.

It is very easy to write a bench whose `cpu_record` is dominated by
`std::cos` or `std::rand`, not by the system you wanted to measure.

**Symptom**: `cpu_record` doesn't change when you optimize the
renderer; or it changes a lot when you replace `std::cos` with a
table.

**Fix**: make scene generation as cheap as possible. The line-renderer
bench uses a deterministic spiral with a precomputed sin/cos table:
the inner loop is `~6 integer ops + 2 muls + AddLine`. Anything left
in `cpu_record` is the renderer's cost.

Rule of thumb on a modern desktop CPU:
- AddLine on Gecko's `DebugRendererContext`: ~9 ns/line
- One `std::cos` (libm): ~10 ns

If your scene calls `std::cos` 4× per line, the bench is measuring
trig, not lines.

## Pitfall #2: implicit GPU sync hides parallelism

CPU and GPU are supposed to run concurrently:
`frame_total ≈ max(cpu_phase, gpu_phase)`. In practice we usually
see `frame_total ≈ cpu_phase + gpu_phase` because of:

1. **GPU profiler timestamp readback** -- to fill the
   `gpu_draw_lines` metric, the host must wait for the GPU to finish
   that draw and read back a timestamp query. With a single
   in-flight frame, this serializes the iteration.
2. **Swapchain throttling** -- with a small swapchain image count
   and VSync off, `vkAcquireNextImageKHR` will still block when the
   GPU hasn't released a back buffer.
3. **Frames-in-flight = 1** -- the next `BeginFrame` waits on the
   `InFlight` fence, so the iteration cannot start until the
   previous frame's GPU work is done.

**Symptom**: a CPU-side optimization that should free 2 ms of CPU
work doesn't drop `frame_total` at all -- the saved time just
appears as a bigger gap before the next iteration.

**How to spot it**: compare `frame_total` to `cpu_record + cmd_submit
+ gpu_draw_lines`. If they're roughly equal, you are serialized. If
`frame_total` is closer to `max(cpu, gpu)`, you have real overlap.

## Pitfall #3: `cmd_submit` is mostly waiting

`vkQueueSubmit` itself is fast (microseconds). When `cmd_submit`
shows milliseconds, what's actually happening is the queue is full
and the call blocks until the GPU drains enough to accept new work.

**Don't** read `cmd_submit` as "submission overhead". Read it as
**"how long the host stalled trying to push work to the GPU"** --
it's a *queue pressure* signal.

The async fence-tracked `UploadBufferData` rewrite (commit
`graphics(vulkan): async fence-tracked UploadBufferData`) is a good
example: dropping `vkQueueWaitIdle` from the upload path cut
`cmd_submit` by 33-89 % across Linux and pi, even though the actual
upload is the same number of bytes.

## Pitfall #4: bigger scenes are not always slower per line

The bench's `lines_sweep` deliberately uses a deterministic spiral
that *increases overdraw at the centre as the line count grows*.
Per-line CPU cost stays flat (~9 ns/line) but per-line GPU cost
grows because more fragments overlap. So `gpu_draw_lines` scales
super-linearly while `cpu_record` scales linearly.

That is a feature: it shows where the wall is. If you only ever
benched scattered, non-overlapping lines, you'd never see the
fragment-fillrate cliff that real debug overlays hit.

## Reading the HTML Report

`gk bench results <program>` opens an HTML report.

- **Top toolbar -- display mode (`time` / `rate`)**: any time-domain
  metric (`ns`) can be flipped to `Hz` (`1e9 / value`). Useful when
  you want to read "how many lines per second" rather than "ns per
  line". `Hz` units are shown raw, never auto-scaled to `kHz` /
  `MHz` (which made comparisons confusing).
- **Per-case toolbar**:
  - `metric` selector: which timing series is plotted.
  - `log` button: log Y axis for sweeps that span 100×+.
  - source pill: `cpu` / `gpu` / `user` -- where the timestamp came from.
- **Sweep view**:
  - Line chart of mean per run, with a min/max band.
  - Drill slider underneath snaps to a single sweep point and
    shows a min/mean/max bar chart for every run at that point.

## How to Run a Comparison

Standard before/after cycle:

```bash
# Baseline
gk bench run debug_renderer -o line_renderer/baseline

# Make a change in src/...

# After
gk bench run debug_renderer -o line_renderer/after

# Compare
gk bench results debug_renderer/line_renderer
```

Numbers from a single run can swing ±5 % from background load. For a
real verdict, capture three runs of each variant and compare medians:

```bash
for i in 1 2 3; do
  gk bench run debug_renderer -o line_renderer/baseline_$i
done
```

## See Also

- [Bench HUD timing cheatsheet](../copilot_context/HUD_TIMING_CHEATSHEET.md)
- [Profiler design notes](../copilot_context/PROFILER_V2_POSTMORTEM.md)
