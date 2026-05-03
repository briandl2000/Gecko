"""`gk bench` -- build, run, and view Gecko benchmarks.

Subcommands:
  gk bench run <program> [<case>] [-o <run_name>]
  gk bench list [<program>]
  gk bench results <program> [-o <out.html>]

Output layout:
  bench_results/<program>/<run_name>.json
  /tmp/gk_bench_<...>.html              (when `results` runs without -o)
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import time
import webbrowser
from pathlib import Path

from scripts.commands import BUILD_DIR, OUTPUT_DIR, _REPO_ROOT, _is_windows
from scripts.commands.build import _auto_configure


def _discover_programs() -> list[str]:
    bench_root = Path(_REPO_ROOT) / "bench"
    if not bench_root.is_dir():
        return []
    return sorted(
        d.name
        for d in bench_root.iterdir()
        if d.is_dir() and (d / "CMakeLists.txt").exists()
    )


def _bench_target(program: str) -> str:
    return f"bench_{program}"


def _bench_executable(program: str, config: str) -> Path:
    exe_suffix = ".exe" if _is_windows() else ""
    return Path(
        f"{OUTPUT_DIR}/bin/{config}/bench/{_bench_target(program)}{exe_suffix}"
    )


def _ensure_benchmarks_enabled() -> int:
    build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)
    cache_file = os.path.join(BUILD_DIR, "CMakeCache.txt")
    if not os.path.isfile(cache_file):
        rc = _auto_configure()
        if rc != 0:
            return rc
    enabled = False
    if os.path.isfile(cache_file):
        with open(cache_file) as f:
            for line in f:
                if line.strip() == "GECKO_BUILD_BENCHMARKS:BOOL=ON":
                    enabled = True
                    break
    if enabled:
        return 0
    print("Enabling benchmarks in build configuration...")
    return subprocess.run(
        ["cmake", "-B", build_dir_rel, "-DGECKO_BUILD_BENCHMARKS=ON"],
        cwd=_REPO_ROOT, check=False,
    ).returncode


def _git_hash() -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=_REPO_ROOT,
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip()
    except Exception:
        return ""


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "bench",
        help="Build, run, and view Gecko benchmarks (Release by default).",
    )
    sub = parser.add_subparsers(dest="bench_cmd")

    run = sub.add_parser("run", help="Run a benchmark program.")
    run.add_argument("program", help="Bench program (e.g. debug_renderer)")
    run.add_argument("case", nargs="?", help="Run only this case (optional)")
    run.add_argument("-o", "--out", default="_latest",
                     help="Run name (default: _latest). File is "
                          "bench_results/<program>/<name>.json")
    run.add_argument("--config", default="release",
                     choices=["debug", "release"],
                     help="Build configuration. Default: release")
    run.add_argument("--build-only", action="store_true",
                     help="Build the bench binary, don't run it.")
    run.add_argument("--iters", type=int, default=None,
                     help="Override measured iteration count.")
    run.add_argument("--warmup", type=int, default=None,
                     help="Override warmup iteration count.")
    run.set_defaults(handler=_run)

    lst = sub.add_parser(
        "list", help="List benchmark programs and registered cases.")
    lst.add_argument("program", nargs="?",
                     help="Show cases of this program.")
    lst.add_argument("--config", default="release",
                     choices=["debug", "release"])
    lst.set_defaults(handler=_list)

    res = sub.add_parser(
        "results",
        help="View all runs for a program as an HTML report. Opens a "
             "browser by default; pass -o to save to disk instead.")
    res.add_argument("program", help="Bench program")
    res.add_argument("-o", "--out", default=None,
                     help="HTML output path (default: open in browser).")
    res.set_defaults(handler=_results)

    parser.set_defaults(handler=_dispatch)


def _dispatch(args) -> int:
    print("Usage: gk bench <run|list|results> ...   (try 'gk bench --help')")
    return 1


def _run(args) -> int:
    programs = _discover_programs()
    if args.program not in programs:
        print(f"Unknown bench program: {args.program}")
        if programs:
            print("Available:", ", ".join(programs))
        return 2

    config = "Debug" if args.config == "debug" else "Release"
    rc = _ensure_benchmarks_enabled()
    if rc != 0:
        return rc

    target = _bench_target(args.program)
    build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)

    print(f"Building {target} ({config})...")
    rc = subprocess.run(
        ["cmake", "--build", build_dir_rel, "--config", config,
         "--target", target],
        cwd=_REPO_ROOT, check=False,
    ).returncode
    if rc != 0:
        return rc

    exe = _bench_executable(args.program, config)
    if not exe.exists():
        print(f"Bench binary not found: {exe}")
        return 1
    if args.build_only:
        print(f"Built {exe}")
        return 0

    out_path = (Path(_REPO_ROOT) / "bench_results"
                / args.program / f"{args.out}.json")
    out_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [str(exe), "--out", str(out_path),
           "--program", args.program,
           "--git", _git_hash()]
    if args.case:
        cmd += ["--case", args.case]
    if args.iters is not None:
        cmd += ["--iters", str(args.iters)]
    if args.warmup is not None:
        cmd += ["--warmup", str(args.warmup)]

    rel = out_path.relative_to(_REPO_ROOT)
    print(f"Running {exe.name} -> {rel}")
    return subprocess.run(
        cmd, cwd=str(Path(_REPO_ROOT) / "working_dir"), check=False,
    ).returncode


def _list(args) -> int:
    programs = _discover_programs()
    if args.program is None:
        for p in programs:
            print(p)
        return 0
    if args.program not in programs:
        print(f"Unknown bench program: {args.program}")
        return 2

    config = "Debug" if args.config == "debug" else "Release"
    rc = _ensure_benchmarks_enabled()
    if rc != 0:
        return rc

    target = _bench_target(args.program)
    build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)
    rc = subprocess.run(
        ["cmake", "--build", build_dir_rel, "--config", config,
         "--target", target],
        cwd=_REPO_ROOT, check=False,
    ).returncode
    if rc != 0:
        return rc

    exe = _bench_executable(args.program, config)
    if not exe.exists():
        print(f"Bench binary not found: {exe}")
        return 1
    return subprocess.run([str(exe), "--list"], check=False).returncode


def _results(args) -> int:
    bench_results = Path(_REPO_ROOT) / "bench_results" / args.program
    if not bench_results.is_dir():
        print(f"No results for '{args.program}'. Run "
              f"`gk bench run {args.program}` first.")
        return 1

    runs = sorted(bench_results.glob("*.json"))
    if not runs:
        print(f"No JSON files in {bench_results}.")
        return 1

    if args.out is None:
        ts = time.strftime("%Y%m%d_%H%M%S")
        out = Path(tempfile.gettempdir()) / f"gk_bench_{args.program}_{ts}.html"
        open_in_browser = True
    else:
        out = Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        open_in_browser = False

    report_script = Path(_REPO_ROOT) / "scripts" / "bench_report.py"
    rc = subprocess.run(
        [sys.executable, str(report_script),
         "--out", str(out),
         "--program", args.program,
         "--"] + [str(p) for p in runs],
        check=False,
    ).returncode
    if rc != 0:
        return rc

    if open_in_browser:
        url = out.resolve().as_uri()
        print(f"Opening {url}")
        webbrowser.open(url)
    return 0
