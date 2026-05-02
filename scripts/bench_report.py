#!/usr/bin/env python3
"""Generate an HTML comparison report from two benchmark run directories.

Usage:
    bench_report.py <run_a_dir> <run_b_dir> --out <out.html>

Each run dir is expected to contain one or more `<program>.json` files
written by the gecko::bench harness. Programs that exist in both runs
are diffed; programs in only one are reported as missing.

The report uses chart.js from CDN. Each case becomes a card with:
  - Stats table (mean / p50 / p95 / min / max for each run, with delta %)
  - A line chart of per-iteration samples (overlay of run_a and run_b)
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _load_run(run_dir: Path) -> dict[str, dict]:
    """Return {program_name: parsed_json}."""
    if not run_dir.is_dir():
        print(f"Run dir not found: {run_dir}", file=sys.stderr)
        return {}
    out = {}
    for f in sorted(run_dir.glob("*.json")):
        try:
            with open(f) as fp:
                out[f.stem] = json.load(fp)
        except Exception as e:
            print(f"Skipping {f}: {e}", file=sys.stderr)
    return out


def _case_key(case: dict) -> str:
    """Stable key for a case (name + sorted args)."""
    args = case.get("args", {}) or {}
    args_str = ",".join(f"{k}={v}" for k, v in sorted(args.items()))
    return f"{case['name']}({args_str})" if args_str else case["name"]


def _delta_pct(a: float, b: float) -> float:
    if a == 0:
        return 0.0
    return (b - a) / a * 100.0


def _ns_human(ns: int) -> str:
    if ns >= 1_000_000_000:
        return f"{ns/1e9:.3f} s"
    if ns >= 1_000_000:
        return f"{ns/1e6:.3f} ms"
    if ns >= 1_000:
        return f"{ns/1e3:.3f} us"
    return f"{ns} ns"


HTML_TEMPLATE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Gecko bench: {title}</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
  body {{ font-family: -apple-system, Segoe UI, Roboto, sans-serif;
         background: #111; color: #ddd; margin: 0; padding: 24px; }}
  h1 {{ font-size: 22px; margin: 0 0 4px 0; }}
  h2 {{ font-size: 18px; margin: 24px 0 8px 0; color: #9cf; }}
  .meta {{ font-size: 13px; color: #888; margin-bottom: 16px; }}
  .case {{ background: #1c1c1c; border: 1px solid #333; border-radius: 6px;
           padding: 16px; margin-bottom: 18px; }}
  .case h3 {{ margin: 0 0 8px 0; font-size: 16px; color: #fff; }}
  table {{ border-collapse: collapse; width: 100%; font-size: 13px; }}
  th, td {{ padding: 5px 10px; text-align: right; border-bottom: 1px solid #333; }}
  th:first-child, td:first-child {{ text-align: left; }}
  th {{ color: #aaa; font-weight: 500; }}
  .pos {{ color: #f77; }}
  .neg {{ color: #7f7; }}
  .neutral {{ color: #aaa; }}
  .chart-wrap {{ margin-top: 12px; height: 220px; }}
  .legend {{ display: inline-block; padding: 2px 8px; border-radius: 3px;
             font-size: 11px; margin-right: 6px; }}
  .a {{ background: #345d8b; }}
  .b {{ background: #6b3a8b; }}
  .missing {{ color: #f88; font-style: italic; }}
</style>
</head>
<body>
<h1>{title}</h1>
<div class="meta">
  <span class="legend a">A</span>{a_label} &nbsp;
  <span class="legend b">B</span>{b_label}
</div>
{body}
<script>
const charts = {charts_json};
for (const [id, cfg] of Object.entries(charts)) {{
  const ctx = document.getElementById(id).getContext('2d');
  new Chart(ctx, cfg);
}}
</script>
</body>
</html>
"""


def _delta_class(pct: float) -> str:
    # Lower is better -> negative delta is good (green).
    if pct < -2:
        return "neg"
    if pct > 2:
        return "pos"
    return "neutral"


def _render_program(prog: str, run_a: dict, run_b: dict) -> tuple[str, dict]:
    """Returns (html_body, charts_dict)."""
    body = [f'<h2>{prog}</h2>']
    charts: dict[str, dict] = {}

    cases_a = {_case_key(c): c for c in run_a.get("cases", [])} if run_a else {}
    cases_b = {_case_key(c): c for c in run_b.get("cases", [])} if run_b else {}

    if not run_a:
        body.append('<div class="missing">missing in A</div>')
    if not run_b:
        body.append('<div class="missing">missing in B</div>')

    keys = sorted(set(cases_a) | set(cases_b))
    for k in keys:
        a = cases_a.get(k)
        b = cases_b.get(k)
        body.append('<div class="case">')
        body.append(f'<h3>{k}</h3>')

        body.append('<table>')
        body.append('<tr><th>metric</th><th>A</th><th>B</th><th>delta</th></tr>')
        for label, key in [("mean", "mean"), ("p50", "p50"), ("p95", "p95"),
                           ("min", "min"), ("max", "max"),
                           ("stddev", "stddev")]:
            av = (a or {}).get("stats_ns", {}).get(key, 0)
            bv = (b or {}).get("stats_ns", {}).get(key, 0)
            d = _delta_pct(av, bv) if (av and bv) else 0.0
            cls = _delta_class(d)
            d_str = f"{d:+.1f}%" if av and bv else "-"
            body.append(f'<tr><td>{label}</td>'
                        f'<td>{_ns_human(av) if av else "-"}</td>'
                        f'<td>{_ns_human(bv) if bv else "-"}</td>'
                        f'<td class="{cls}">{d_str}</td></tr>')

        # Sections
        sections_a = (a or {}).get("sections", {}) or {}
        sections_b = (b or {}).get("sections", {}) or {}
        sec_keys = sorted(set(sections_a) | set(sections_b))
        for sk in sec_keys:
            am = sections_a.get(sk, {}).get("mean_ns", 0)
            bm = sections_b.get(sk, {}).get("mean_ns", 0)
            d = _delta_pct(am, bm) if (am and bm) else 0.0
            cls = _delta_class(d)
            d_str = f"{d:+.1f}%" if am and bm else "-"
            body.append(f'<tr><td>section: {sk}</td>'
                        f'<td>{_ns_human(am) if am else "-"}</td>'
                        f'<td>{_ns_human(bm) if bm else "-"}</td>'
                        f'<td class="{cls}">{d_str}</td></tr>')
        body.append('</table>')

        # Per-iteration sample chart
        chart_id = f"chart_{prog}_{k}".replace("(", "_").replace(")", "").replace("=", "_").replace(",", "_").replace(" ", "_")
        body.append(f'<div class="chart-wrap"><canvas id="{chart_id}"></canvas></div>')

        samples_a = (a or {}).get("samples_ns", []) or []
        samples_b = (b or {}).get("samples_ns", []) or []
        max_n = max(len(samples_a), len(samples_b))
        labels = list(range(max_n))
        ds = []
        if samples_a:
            ds.append({"label": "A", "data": [s/1e6 for s in samples_a],
                       "borderColor": "#5599dd", "tension": 0.1, "pointRadius": 0})
        if samples_b:
            ds.append({"label": "B", "data": [s/1e6 for s in samples_b],
                       "borderColor": "#aa66cc", "tension": 0.1, "pointRadius": 0})
        charts[chart_id] = {
            "type": "line",
            "data": {"labels": labels, "datasets": ds},
            "options": {
                "responsive": True, "maintainAspectRatio": False,
                "scales": {
                    "x": {"title": {"display": True, "text": "iteration"},
                          "ticks": {"color": "#888"}, "grid": {"color": "#333"}},
                    "y": {"title": {"display": True, "text": "ms / iter"},
                          "ticks": {"color": "#888"}, "grid": {"color": "#333"}},
                },
                "plugins": {"legend": {"labels": {"color": "#ddd"}}},
            },
        }

        body.append('</div>')
    return "\n".join(body), charts


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("run_a", type=Path)
    p.add_argument("run_b", type=Path)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args(argv)

    run_a = _load_run(args.run_a)
    run_b = _load_run(args.run_b)

    title = f"{args.run_a.name}  vs  {args.run_b.name}"
    a_label = args.run_a.as_posix()
    b_label = args.run_b.as_posix()

    progs = sorted(set(run_a) | set(run_b))
    body_parts: list[str] = []
    all_charts: dict[str, dict] = {}
    for prog in progs:
        h, ch = _render_program(prog, run_a.get(prog), run_b.get(prog))
        body_parts.append(h)
        all_charts.update(ch)

    html = HTML_TEMPLATE.format(
        title=title, a_label=a_label, b_label=b_label,
        body="\n".join(body_parts),
        charts_json=json.dumps(all_charts),
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(html, encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
