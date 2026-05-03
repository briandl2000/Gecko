#!/usr/bin/env python3
"""Generate an HTML report from one or more Gecko bench result files.

Each JSON file is one run of one program. The report:

  - Groups by case name + sweep args.
  - Shows one chart per metric (frame_total, cpu_record, gpu_*, ...).
  - For cases WITHOUT a sweep: per-iteration line chart (one series
    per run).
  - For cases WITH a sweep: grouped bar chart (X = sweep value,
    one bar group per run, bars for min/mean/p95).

Usage:
    bench_report.py --out <out.html> --program <name> -- <run1.json> [<run2.json> ...]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _load(path: Path) -> dict | None:
    try:
        with open(path) as f:
            return json.load(f)
    except Exception as e:
        print(f"Skipping {path}: {e}", file=sys.stderr)
        return None


def _ns_human(ns: float) -> str:
    if ns >= 1_000_000_000:
        return f"{ns/1e9:.2f} s"
    if ns >= 1_000_000:
        return f"{ns/1e6:.2f} ms"
    if ns >= 1_000:
        return f"{ns/1e3:.2f} us"
    return f"{ns:.0f} ns"


def _case_id(case: dict) -> str:
    return case["name"]


def _point_key(case: dict) -> str:
    args = case.get("args", {}) or {}
    return ",".join(f"{k}={v}" for k, v in sorted(args.items()))


HTML = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Gecko bench: {title}</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
  body {{ font-family: -apple-system, Segoe UI, Roboto, sans-serif;
         background: #111; color: #ddd; margin: 0; padding: 24px; }}
  h1 {{ font-size: 22px; margin: 0 0 4px 0; }}
  h2 {{ font-size: 18px; margin: 24px 0 6px 0; color: #9cf; }}
  h3 {{ font-size: 15px; margin: 10px 0 4px 0; color: #ddd; }}
  .runs {{ display: flex; flex-wrap: wrap; gap: 6px;
           font-size: 12px; margin-bottom: 16px; color: #aaa; }}
  .runs .pill {{ padding: 2px 8px; border-radius: 3px;
                 background: #2a2a2a; }}
  .case {{ background: #1c1c1c; border: 1px solid #2a2a2a;
           border-radius: 6px; padding: 14px; margin-bottom: 16px; }}
  table.summary {{ border-collapse: collapse; font-size: 12px;
                   margin: 4px 0 12px 0; }}
  table.summary th, table.summary td {{
      padding: 4px 10px; border-bottom: 1px solid #2a2a2a;
      text-align: right; }}
  table.summary th:first-child, table.summary td:first-child {{
      text-align: left; }}
  table.summary th {{ color: #aaa; font-weight: 500; }}
  .metric {{ margin: 10px 0 18px 0; }}
  .chart-wrap {{ height: 220px; margin-top: 6px; }}
  .src-cpu {{ color: #aaf; font-size: 11px; }}
  .src-gpu {{ color: #faa; font-size: 11px; }}
  .controls {{ font-size: 12px; color: #888; margin: 8px 0; }}
  .controls input {{ margin-right: 6px; }}
  .empty {{ color: #666; font-style: italic; }}
</style>
</head>
<body>
<h1>{title}</h1>
<div class="runs">{runs_html}</div>
<div class="controls">
  <label><input type="checkbox" id="show-samples" checked>
    show per-iteration samples (line view only)</label>
</div>
{body}
<script>
const PALETTE = ['#5599dd', '#aa66cc', '#88cc66', '#ddaa44', '#dd5577',
                 '#33aaaa', '#cccc55', '#bb88dd', '#669966'];
const CHARTS = {charts_json};
const INSTANCES = {{}};
function makeChart(id, cfg) {{
  const ctx = document.getElementById(id);
  if (!ctx) return;
  if (INSTANCES[id]) INSTANCES[id].destroy();
  INSTANCES[id] = new Chart(ctx.getContext('2d'), cfg);
}}
for (const [id, cfg] of Object.entries(CHARTS)) makeChart(id, cfg);
document.getElementById('show-samples').addEventListener('change', () => {{
  // No-op for bar charts; line charts already always visible. Reserved
  // for future toggle of raw samples vs smoothed mean.
}});
</script>
</body>
</html>
"""


def _build_metric_table(metric_data: list[tuple[str, dict | None]]) -> str:
    """metric_data: list of (run_name, metric_dict_or_None)."""
    rows = ["<tr><th>run</th><th>min</th><th>mean</th><th>p50</th>"
            "<th>p95</th><th>max</th><th>stddev</th></tr>"]
    for run_name, m in metric_data:
        if m is None:
            rows.append(f"<tr><td>{run_name}</td>"
                        f"<td colspan=6 class='empty'>n/a</td></tr>")
            continue
        st = m.get("stats_ns", {})
        rows.append(
            "<tr>"
            f"<td>{run_name}</td>"
            f"<td>{_ns_human(st.get('min', 0))}</td>"
            f"<td>{_ns_human(st.get('mean', 0))}</td>"
            f"<td>{_ns_human(st.get('p50', 0))}</td>"
            f"<td>{_ns_human(st.get('p95', 0))}</td>"
            f"<td>{_ns_human(st.get('max', 0))}</td>"
            f"<td>{_ns_human(st.get('stddev', 0))}</td>"
            "</tr>"
        )
    return "<table class='summary'>" + "".join(rows) + "</table>"


def _line_chart(runs: list[tuple[str, dict]], metric_name: str) -> dict:
    """One line per run; X = iteration index; Y = ms."""
    datasets = []
    max_n = 0
    for i, (run_name, m) in enumerate(runs):
        samples = m.get("samples_ns", [])
        max_n = max(max_n, len(samples))
        datasets.append({
            "label": run_name,
            "data": [v / 1e6 for v in samples],
            "borderColor": PALETTE_PY[i % len(PALETTE_PY)],
            "backgroundColor": PALETTE_PY[i % len(PALETTE_PY)],
            "tension": 0.1, "pointRadius": 0,
        })
    return {
        "type": "line",
        "data": {"labels": list(range(max_n)), "datasets": datasets},
        "options": {
            "responsive": True, "maintainAspectRatio": False,
            "scales": {
                "x": {"title": {"display": True, "text": "iteration"},
                      "ticks": {"color": "#888"}, "grid": {"color": "#2a2a2a"}},
                "y": {"title": {"display": True, "text": "ms / iter"},
                      "ticks": {"color": "#888"}, "grid": {"color": "#2a2a2a"}},
            },
            "plugins": {"legend": {"labels": {"color": "#ddd"}}},
        },
    }


def _bar_chart(sweep_axis: str,
               sweep_values: list,
               per_run: list[tuple[str, list[dict | None]]],
               kind: str = "mean") -> dict:
    """X = sweep value; one bar group per run; bars show <kind>."""
    datasets = []
    for i, (run_name, metrics) in enumerate(per_run):
        data = []
        for m in metrics:
            if m is None:
                data.append(None)
            else:
                data.append(m.get("stats_ns", {}).get(kind, 0) / 1e6)
        datasets.append({
            "label": f"{run_name} ({kind})",
            "data": data,
            "backgroundColor": PALETTE_PY[i % len(PALETTE_PY)],
            "borderColor": PALETTE_PY[i % len(PALETTE_PY)],
        })
    return {
        "type": "bar",
        "data": {"labels": [str(v) for v in sweep_values],
                 "datasets": datasets},
        "options": {
            "responsive": True, "maintainAspectRatio": False,
            "scales": {
                "x": {"title": {"display": True, "text": sweep_axis},
                      "ticks": {"color": "#888"}, "grid": {"color": "#2a2a2a"}},
                "y": {"title": {"display": True, "text": f"ms ({kind})"},
                      "ticks": {"color": "#888"}, "grid": {"color": "#2a2a2a"}},
            },
            "plugins": {"legend": {"labels": {"color": "#ddd"}}},
        },
    }


PALETTE_PY = ['#5599dd', '#aa66cc', '#88cc66', '#ddaa44', '#dd5577',
              '#33aaaa', '#cccc55', '#bb88dd', '#669966']


def _safe_id(s: str) -> str:
    out = []
    for c in s:
        if c.isalnum() or c == '_':
            out.append(c)
        else:
            out.append('_')
    return ''.join(out)


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--program", required=True,
                   help="Program name (used in title).")
    p.add_argument("runs", nargs="+",
                   help="Path(s) to run JSON file(s).")
    args = p.parse_args(argv)

    # Load runs.
    loaded: list[tuple[str, dict]] = []  # [(run_name, json), ...]
    for r in args.runs:
        rp = Path(r)
        data = _load(rp)
        if data is None:
            continue
        loaded.append((rp.stem, data))
    if not loaded:
        print("No runs loaded.", file=sys.stderr)
        return 1

    # Collect cases. A "case_id" groups across runs by name; within each
    # case_id we further group by sweep args ("point_key").
    # Layout:
    #   case_groups[case_name] = {
    #       "view": "lines"|"bars",
    #       "sweep_axis": str|None,
    #       "points": {point_key: {"args": {...}, "runs": {run_name: case_dict}}}
    #   }
    case_groups: dict[str, dict] = {}
    for run_name, data in loaded:
        for case in data.get("cases", []):
            cid = _case_id(case)
            g = case_groups.setdefault(cid, {
                "view": case.get("view", "lines"),
                "sweep_axis": None,
                "description": case.get("description", ""),
                "points": {},
            })
            args_dict = case.get("args", {}) or {}
            if args_dict and g["sweep_axis"] is None:
                g["sweep_axis"] = sorted(args_dict.keys())[0]
            pk = _point_key(case)
            pt = g["points"].setdefault(pk, {"args": args_dict, "runs": {}})
            pt["runs"][run_name] = case

    # Build HTML body.
    runs_html = "".join(
        f"<span class='pill'>{rn}</span>" for rn, _ in loaded)

    body_parts: list[str] = []
    charts: dict[str, dict] = {}

    for cid, g in sorted(case_groups.items()):
        body_parts.append("<div class='case'>")
        body_parts.append(f"<h2>{cid}</h2>")
        if g["description"]:
            body_parts.append(
                f"<div class='controls'>{g['description']}</div>")

        view = g["view"]
        # Discover all metric names across runs/points.
        all_metrics: list[tuple[str, str]] = []  # (name, source)
        seen: set[str] = set()
        for pk, pt in g["points"].items():
            for run_name, case in pt["runs"].items():
                for mname, m in (case.get("metrics", {}) or {}).items():
                    if mname not in seen:
                        seen.add(mname)
                        all_metrics.append((mname, m.get("source", "cpu")))

        if view == "bars" and g["sweep_axis"]:
            # Sort points by sweep value.
            sweep_axis = g["sweep_axis"]
            sorted_points = sorted(
                g["points"].items(),
                key=lambda kv: kv[1]["args"].get(sweep_axis, 0))
            sweep_values = [pt["args"].get(sweep_axis)
                            for _, pt in sorted_points]
            for mname, source in all_metrics:
                # Build per_run = [(run_name, [metric_for_each_sweep_value])]
                per_run: list[tuple[str, list[dict | None]]] = []
                for run_name, _ in loaded:
                    metrics_per_pt = []
                    for _, pt in sorted_points:
                        case = pt["runs"].get(run_name)
                        if case is None:
                            metrics_per_pt.append(None)
                        else:
                            metrics_per_pt.append(
                                (case.get("metrics", {}) or {}).get(mname))
                    per_run.append((run_name, metrics_per_pt))

                src_cls = "src-gpu" if source == "gpu" else "src-cpu"
                body_parts.append("<div class='metric'>")
                body_parts.append(
                    f"<h3>{mname} <span class='{src_cls}'>"
                    f"({source})</span></h3>")
                # Three bar charts: min, mean, p95.
                for kind in ("min", "mean", "p95"):
                    chart_id = _safe_id(f"c_{cid}_{mname}_{kind}")
                    body_parts.append(
                        f"<div class='chart-wrap'>"
                        f"<canvas id='{chart_id}'></canvas></div>")
                    charts[chart_id] = _bar_chart(
                        sweep_axis, sweep_values, per_run, kind)
                body_parts.append("</div>")

        else:
            # Line view: only one point (no sweep) -- if sweep present
            # under view=lines, take the first point.
            if not g["points"]:
                body_parts.append("<div class='empty'>no data</div>")
                body_parts.append("</div>")
                continue

            # Pick the lone point (or first if multiple).
            pk, pt = next(iter(sorted(g["points"].items())))
            for mname, source in all_metrics:
                per_run_metric: list[tuple[str, dict]] = []
                summary_data: list[tuple[str, dict | None]] = []
                for run_name, _ in loaded:
                    case = pt["runs"].get(run_name)
                    m = (case or {}).get("metrics", {}).get(mname)
                    summary_data.append((run_name, m))
                    if m is not None:
                        per_run_metric.append((run_name, m))

                src_cls = "src-gpu" if source == "gpu" else "src-cpu"
                body_parts.append("<div class='metric'>")
                body_parts.append(
                    f"<h3>{mname} <span class='{src_cls}'>"
                    f"({source})</span></h3>")
                body_parts.append(_build_metric_table(summary_data))
                if per_run_metric:
                    chart_id = _safe_id(f"c_{cid}_{mname}_lines")
                    body_parts.append(
                        f"<div class='chart-wrap'>"
                        f"<canvas id='{chart_id}'></canvas></div>")
                    charts[chart_id] = _line_chart(per_run_metric, mname)
                body_parts.append("</div>")

        body_parts.append("</div>")  # /case

    title = f"{args.program}"
    html = HTML.format(
        title=title,
        runs_html=runs_html,
        body="\n".join(body_parts),
        charts_json=json.dumps(charts),
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(html, encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
