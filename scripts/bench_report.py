#!/usr/bin/env python3
"""Generate an HTML report from one or more Gecko bench result files.

One big chart per case + a dropdown metric selector. Two layouts:

  * static (no sweep)  -> grouped bar chart.
                          X = run, datasets = min / mean / max.
  * swept (one sweep axis present) -> line chart.
                          X = sweep value, one mean line per run,
                          shaded band = [min, max].

Multi-axis sweeps fall back to bar layout with composite labels.

Usage:
    bench_report.py --out <out.html> --program <name> -- <run1.json> [...]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


# Distinct hues for runs in line charts.
PALETTE = ['#5599dd', '#aa66cc', '#88cc66', '#ddaa44', '#dd5577',
           '#33aaaa', '#cccc55', '#bb88dd', '#669966']

KIND_COLOR = {"min": "#88cc66", "mean": "#5599dd", "max": "#dd5577"}


def _load(path: Path) -> dict | None:
    try:
        with open(path) as f:
            return json.load(f)
    except Exception as e:
        print(f"Skipping {path}: {e}", file=sys.stderr)
        return None


def _safe_id(s: str) -> str:
    return ''.join(c if c.isalnum() or c == '_' else '_' for c in s)


def _hex_to_rgba(hexcol: str, alpha: float) -> str:
    h = hexcol.lstrip('#')
    r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    return f"rgba({r},{g},{b},{alpha:.2f})"


HTML = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Gecko bench: {title}</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
  body {{ font-family: -apple-system, Segoe UI, Roboto, sans-serif;
         background: #111; color: #ddd; margin: 0; padding: 24px;
         max-width: 1100px; margin-left: auto; margin-right: auto; }}
  h1 {{ font-size: 22px; margin: 0 0 4px 0; }}
  h2 {{ font-size: 18px; margin: 0 0 6px 0; color: #9cf; }}
  .runs {{ display: flex; flex-wrap: wrap; gap: 6px;
           font-size: 12px; margin-bottom: 16px; color: #aaa; }}
  .runs .pill {{ padding: 2px 8px; border-radius: 3px;
                 background: #2a2a2a; }}
  .case {{ background: #1c1c1c; border: 1px solid #2a2a2a;
           border-radius: 6px; padding: 16px; margin-bottom: 18px; }}
  .case .desc {{ color: #888; font-size: 12px; margin-bottom: 10px; }}
  .toolbar {{ display: flex; gap: 10px; align-items: center;
              margin-bottom: 8px; font-size: 12px; color: #888; }}
  .toolbar select, .toolbar button {{ background: #222; color: #ddd;
      border: 1px solid #333; padding: 4px 6px; border-radius: 3px;
      font: inherit; cursor: pointer; }}
  .toolbar button.on {{ background: #345; border-color: #456; }}
  .modebar {{ display: flex; gap: 16px; align-items: center;
              font-size: 12px; color: #aaa; margin-bottom: 14px;
              padding: 8px 12px; background: #1a1a1a;
              border: 1px solid #2a2a2a; border-radius: 4px; }}
  .modebar label {{ cursor: pointer; user-select: none; }}
  .modebar input[type=radio] {{ vertical-align: middle; margin-right: 4px; }}
  .src {{ font-size: 11px; padding: 1px 6px; border-radius: 3px; }}
  .src-cpu {{ color: #aaf; background: #1a1a2a; }}
  .src-gpu {{ color: #faa; background: #2a1a1a; }}
  .src-user {{ color: #afa; background: #1a2a1a; }}
  .chart-wrap {{ height: 360px; }}
  .drill {{ margin-top: 14px; border-top: 1px solid #2a2a2a; padding-top: 12px; }}
  .drill .label {{ font-size: 12px; color: #888;
                   display: flex; gap: 10px; align-items: center; }}
  .drill input[type=range] {{ flex: 1; }}
  .drill .val {{ color: #ddd; min-width: 4ch; text-align: right; }}
  .drill-chart {{ height: 220px; margin-top: 6px; }}
</style>
</head>
<body>
<h1>{title}</h1>
<div class="runs">{runs_html}</div>
<div class="modebar">
  <span style="color:#888">display:</span>
  <label><input type="radio" name="mode" value="time" checked onchange="setMode('time')"> time (ns / us / ms / s)</label>
  <label><input type="radio" name="mode" value="rate" onchange="setMode('rate')"> rate (Hz)</label>
  <span style="color:#666;margin-left:auto;font-size:11px">time metrics flip to 1/value when in Rate mode</span>
</div>
{body}
<script>
const CASES = {cases_json};

function rgba(hex, a) {{
  const r = parseInt(hex.slice(1,3),16),
        g = parseInt(hex.slice(3,5),16),
        b = parseInt(hex.slice(5,7),16);
  return `rgba(${{r}},${{g}},${{b}},${{a}})`;
}}

// Stable per-run color: hash the run name -> palette index.
// Same run name always picks the same color regardless of which
// other runs are in view, or what alphabetical position it has.
function hashStr(s) {{
  let h = 2166136261 >>> 0;
  for (let i = 0; i < s.length; ++i) {{
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 16777619) >>> 0;
  }}
  return h;
}}
function colorForRun(name, palette) {{
  return palette[hashStr(name) % palette.length];
}}

// Format a value with a unit suffix for tooltip display.
function fmtVal(v, unit) {{
  if (v == null || !isFinite(v)) return '—';
  const abs = Math.abs(v);
  let digits = 0;
  if (abs < 1) digits = 3;
  else if (abs < 10) digits = 2;
  else if (abs < 100) digits = 1;
  return v.toFixed(digits) + ' ' + (unit || '');
}}

// Pick a presentation unit + scale factor from a metric's raw unit
// and the geometric mean of its values. Returns {{label, scale}}
// such that displayValue = rawValue * scale.
// Returns {{label, transform}} where transform: (rawValue) -> displayValue.
// Honors the global Time / Rate mode for time-domain metrics (ns).
// Hz metrics are always shown as Hz (no kHz / MHz auto-scaling, which
// people find unintuitive when comparing runs).
function pickUnit(rawUnit, samples) {{
  const xs = samples.filter(v => v != null && isFinite(v) && v > 0);
  let mag = 0;
  if (xs.length) {{
    let s = 0;
    for (const v of xs) s += Math.log(v);
    mag = Math.exp(s / xs.length);
  }}
  if (rawUnit === 'ns') {{
    if (MODE.current === 'rate') {{
      return {{ label: 'Hz',
               transform: v => (v == null || v === 0) ? null : 1e9 / v }};
    }}
    if (mag >= 1e9)  return {{ label: 's',  transform: v => v == null ? null : v * 1e-9 }};
    if (mag >= 1e6)  return {{ label: 'ms', transform: v => v == null ? null : v * 1e-6 }};
    if (mag >= 1e3)  return {{ label: 'us', transform: v => v == null ? null : v * 1e-3 }};
    return {{ label: 'ns', transform: v => v }};
  }}
  if (rawUnit === 'hz') {{
    if (MODE.current === 'rate') {{
      return {{ label: 'Hz', transform: v => v }};
    }}
    // In Time mode, an Hz metric becomes its period.
    return {{ label: 'ms', transform: v => (v == null || v === 0) ? null : 1000 / v }};
  }}
  return {{ label: rawUnit || '', transform: v => v }};
}}

function collectAll(metric) {{
  // Flatten everything we'll need to plot for unit-picking purposes.
  const out = [];
  if (metric.points) {{
    for (const p of metric.points) {{
      if (p.min != null) out.push(p.min);
      if (p.mean != null) out.push(p.mean);
      if (p.max != null) out.push(p.max);
    }}
  }}
  if (metric.runs) {{
    for (const r of metric.runs) {{
      for (const v of (r.mean  || [])) if (v != null) out.push(v);
      for (const v of (r.min   || [])) if (v != null) out.push(v);
      for (const v of (r.max   || [])) if (v != null) out.push(v);
    }}
  }}
  return out;
}}

function buildBars(metric, transform, axisLabel, logScale, unitLabel) {{
  const labels = metric.points.map(p => p.x);
  const apply = transform;
  const ds = ['min','mean','max'].map(k => ({{
    label: k,
    data: metric.points.map(p => apply(p[k])),
    backgroundColor: {{min:'#88cc66', mean:'#5599dd', max:'#dd5577'}}[k],
  }}));
  return {{
    type: 'bar',
    data: {{ labels, datasets: ds }},
    options: {{
      responsive: true, maintainAspectRatio: false, animation: false,
      scales: {{
        x: {{ ticks: {{ color: '#aaa' }}, grid: {{ color: '#252525' }} }},
        y: {{
          type: logScale ? 'logarithmic' : 'linear',
          title: {{ display: true, text: axisLabel, color: '#888' }},
          ticks: {{ color: '#aaa' }}, grid: {{ color: '#252525' }},
        }},
      }},
      plugins: {{
        legend: {{ labels: {{ color: '#ddd' }} }},
        tooltip: {{
          callbacks: {{
            label: (ctx) => `${{ctx.dataset.label}}: ${{fmtVal(ctx.parsed.y, unitLabel)}}`,
          }},
        }},
      }},
    }},
  }};
}}

function buildBarsAtSweep(metric, sweepIdx, transform, axisLabel, logScale, unitLabel) {{
  const points = metric.runs.map(r => ({{
    x: r.run,
    min: r.min[sweepIdx],
    mean: r.mean[sweepIdx],
    max: r.max[sweepIdx],
  }}));
  return buildBars({{ points }}, transform, axisLabel, logScale, unitLabel);
}}

function buildLines(metric, transform, axisLabel, logScale, unitLabel) {{
  const labels = metric.sweep_values;
  const palette = {palette_json};
  const apply = transform;
  const datasets = metric.runs.map((r) => {{
    const c = colorForRun(r.run, palette);
    return {{
      label: r.run,
      data: r.mean.map(apply),
      borderColor: c,
      backgroundColor: c,
      fill: false,
      pointRadius: 3,
      tension: 0.25,
    }};
  }});
  return {{
    type: 'line',
    data: {{ labels, datasets }},
    options: {{
      responsive: true, maintainAspectRatio: false, animation: false,
      scales: {{
        x: {{ title: {{ display: true, text: metric.sweep_axis, color: '#888' }},
              ticks: {{ color: '#aaa' }}, grid: {{ color: '#252525' }} }},
        y: {{
          type: logScale ? 'logarithmic' : 'linear',
          title: {{ display: true, text: axisLabel, color: '#888' }},
          ticks: {{ color: '#aaa' }}, grid: {{ color: '#252525' }},
        }},
      }},
      plugins: {{
        legend: {{ labels: {{ color: '#ddd' }} }},
        tooltip: {{
          callbacks: {{
            label: (ctx) => `${{ctx.dataset.label}}: ${{fmtVal(ctx.parsed.y, unitLabel)}}`,
          }},
        }},
      }},
    }},
  }};
}}

const ACTIVE = {{}};
const DRILL = {{}};
const LOG_STATE = {{}};
const MODE = {{ current: 'time' }};

function setMode(m) {{
  MODE.current = m;
  for (const id of Object.keys(CASES)) renderMetric(id);
}}

function toggleLog(caseId) {{
  LOG_STATE[caseId] = !LOG_STATE[caseId];
  const btn = document.getElementById('log_' + caseId);
  btn.classList.toggle('on', LOG_STATE[caseId]);
  renderMetric(caseId);
}}

function renderMetric(caseId) {{
  const c = CASES[caseId];
  const sel = document.getElementById('sel_' + caseId);
  const m = c.metrics[sel.value];
  const ctx = document.getElementById('chart_' + caseId);
  if (ACTIVE[caseId]) ACTIVE[caseId].destroy();
  const srcEl = document.getElementById('src_' + caseId);
  srcEl.className = 'src ' +
    (m.source === 'gpu' ? 'src-gpu' :
     m.source === 'user' ? 'src-user' : 'src-cpu');
  srcEl.textContent = m.source;
  const u = pickUnit(m.unit, collectAll(m));
  const stat = c.view === 'lines' ? '(mean)' : '';
  const axisLabel = `${{u.label}} ${{stat}}`.trim();
  const log = !!LOG_STATE[caseId];
  ACTIVE[caseId] = new Chart(ctx.getContext('2d'),
    c.view === 'lines'
      ? buildLines(m, u.transform, axisLabel, log, u.label)
      : buildBars(m, u.transform, axisLabel, log, u.label));
  if (c.view === 'lines') renderDrill(caseId);
}}

function renderDrill(caseId) {{
  const c = CASES[caseId];
  const sel = document.getElementById('sel_' + caseId);
  const m = c.metrics[sel.value];
  const slider = document.getElementById('drill_' + caseId);
  if (!slider) return;
  const idx = parseInt(slider.value, 10);
  const v = m.sweep_values[idx];
  document.getElementById('drillVal_' + caseId).textContent =
    `${{m.sweep_axis}} = ${{v}}`;
  const ctx = document.getElementById('drillChart_' + caseId);
  if (DRILL[caseId]) DRILL[caseId].destroy();
  const u = pickUnit(m.unit, collectAll(m));
  const log = !!LOG_STATE[caseId];
  DRILL[caseId] = new Chart(ctx.getContext('2d'),
    buildBarsAtSweep(m, idx, u.transform, u.label, log, u.label));
}}

for (const id of Object.keys(CASES)) renderMetric(id);
</script>
</body>
</html>
"""


def _composite_arg_label(args: dict) -> str:
    if not args:
        return ""
    return ", ".join(f"{k}={v}" for k, v in sorted(args.items()))


def _build_static_metric(metric_name: str,
                         points: list[dict]) -> dict:
    """Bar layout. One bar group per (run, args)."""
    out_pts = []
    src = "cpu"
    unit = "ns"
    for p in points:
        m = p["metrics"].get(metric_name)
        if m is None:
            continue
        src = m.get("source", src)
        unit = m.get("unit", unit)
        stats = m.get("stats") or m.get("stats_ns", {})
        x = p["run"]
        if p["args"]:
            x = f"{p['run']} [{_composite_arg_label(p['args'])}]"
        out_pts.append({
            "x": x,
            "min": stats.get("min", 0),
            "mean": stats.get("mean", 0),
            "max": stats.get("max", 0),
        })
    return {"source": src, "unit": unit, "points": out_pts}


def _build_swept_metric(metric_name: str, points: list[dict],
                        sweep_axis: str,
                        sweep_values: list,
                        run_order: list[str]) -> dict:
    """Line layout. One mean-line per run; min/max fill band."""
    src = "cpu"
    unit = "ns"
    # Group by run.
    per_run: dict[str, dict[object, dict]] = {}
    for p in points:
        m = p["metrics"].get(metric_name)
        if m is None:
            continue
        src = m.get("source", src)
        unit = m.get("unit", unit)
        stats = m.get("stats") or m.get("stats_ns", {})
        v = p["args"].get(sweep_axis)
        per_run.setdefault(p["run"], {})[v] = {
            "min": stats.get("min", 0),
            "mean": stats.get("mean", 0),
            "max": stats.get("max", 0),
        }
    runs_out = []
    for run in run_order:
        if run not in per_run:
            continue
        d = per_run[run]
        runs_out.append({
            "run": run,
            "min": [d.get(v, {}).get("min") for v in sweep_values],
            "mean": [d.get(v, {}).get("mean") for v in sweep_values],
            "max": [d.get(v, {}).get("max") for v in sweep_values],
        })
    return {
        "source": src,
        "unit": unit,
        "sweep_axis": sweep_axis,
        "sweep_values": sweep_values,
        "runs": runs_out,
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--program", required=True)
    p.add_argument("runs", nargs="+")
    args = p.parse_args(argv)

    loaded: list[tuple[str, dict]] = []
    for r in args.runs:
        rp = Path(r)
        data = _load(rp)
        if data is None:
            continue
        loaded.append((rp.stem, data))
    if not loaded:
        print("No runs loaded.", file=sys.stderr)
        return 1
    run_order = [rn for rn, _ in loaded]

    # Aggregate by case.
    cases: dict[str, dict] = {}
    for run_name, data in loaded:
        for case in data.get("cases", []):
            cid = case["name"]
            g = cases.setdefault(cid, {
                "description": case.get("description", ""),
                "points": [],
            })
            g["points"].append({
                "run": run_name,
                "args": case.get("args", {}) or {},
                "metrics": case.get("metrics", {}) or {},
            })

    runs_html = "".join(
        f"<span class='pill'>{rn}</span>" for rn in run_order)

    body_parts: list[str] = []
    cases_payload: dict[str, dict] = {}

    for cid, g in sorted(cases.items()):
        # Detect sweep: a single arg key common to all points.
        arg_keys: set[str] = set()
        for p in g["points"]:
            arg_keys.update(p["args"].keys())
        view = "bars"
        sweep_axis: str | None = None
        sweep_values: list = []
        if len(arg_keys) == 1:
            sweep_axis = next(iter(arg_keys))
            vs: list = []
            seen_v: set = set()
            for p in g["points"]:
                v = p["args"].get(sweep_axis)
                if v is not None and v not in seen_v:
                    seen_v.add(v)
                    vs.append(v)
            try:
                vs.sort(key=lambda x: (0, float(x)))
            except (TypeError, ValueError):
                vs.sort(key=str)
            sweep_values = vs
            if len(sweep_values) >= 2:
                view = "lines"

        # Collect metric names (frame_total first).
        metric_names: list[str] = []
        seen: set[str] = set()
        for p in g["points"]:
            for mname in p["metrics"]:
                if mname not in seen:
                    seen.add(mname)
                    metric_names.append(mname)
        metric_names.sort(key=lambda n: (0 if n == "frame_total" else 1, n))
        if not metric_names:
            continue

        # Build per-metric payloads.
        metrics_payload: dict[str, dict] = {}
        for mname in metric_names:
            if view == "lines" and sweep_axis is not None:
                metrics_payload[mname] = _build_swept_metric(
                    mname, g["points"], sweep_axis, sweep_values, run_order)
            else:
                metrics_payload[mname] = _build_static_metric(
                    mname, g["points"])

        cases_payload[cid] = {
            "view": view,
            "metrics": metrics_payload,
        }

        # Default selected metric.
        default_metric = metric_names[0]
        first_src = metrics_payload[default_metric]["source"]
        src_class = "src-gpu" if first_src == "gpu" else "src-cpu"

        opts = "".join(
            f"<option value='{m}'>{m}</option>" for m in metric_names)
        body_parts.append("<div class='case'>")
        body_parts.append(f"<h2>{cid}</h2>")
        if g["description"]:
            body_parts.append(f"<div class='desc'>{g['description']}</div>")
        body_parts.append(
            f"<div class='toolbar'>"
            f"<label for='sel_{cid}'>metric:</label>"
            f"<select id='sel_{cid}' onchange=\"renderMetric('{cid}')\">{opts}</select>"
            f"<span id='src_{cid}' class='src {src_class}'>{first_src}</span>"
            f"<button id='log_{cid}' onclick=\"toggleLog('{cid}')\">log</button>"
            f"<span style='margin-left:auto;color:#666'>{view}</span>"
            f"</div>")
        body_parts.append(
            f"<div class='chart-wrap'><canvas id='chart_{cid}'></canvas></div>")
        if view == "lines":
            n = max(len(sweep_values) - 1, 0)
            init_idx = n  # default to the largest value (rightmost).
            body_parts.append(
                f"<div class='drill'>"
                f"<div class='label'>"
                f"<span>drill into:</span>"
                f"<input type='range' id='drill_{cid}' min='0' max='{n}' "
                f"step='1' value='{init_idx}' "
                f"oninput=\"renderDrill('{cid}')\">"
                f"<span class='val' id='drillVal_{cid}'></span>"
                f"</div>"
                f"<div class='drill-chart'>"
                f"<canvas id='drillChart_{cid}'></canvas></div>"
                f"</div>")
        body_parts.append("</div>")

    html = HTML.format(
        title=args.program,
        runs_html=runs_html,
        body="\n".join(body_parts),
        cases_json=json.dumps(cases_payload),
        palette_json=json.dumps(PALETTE),
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(html, encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
