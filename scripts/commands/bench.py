"""`gk bench` -- build and run benchmark binaries.

Subcommands:
  gk bench <program> [<case>] [-o <run_name>]
  gk bench list [<program>]
  gk bench compare <run_a> <run_b> [-o <html_path>]

Run output goes to:
  <repo>/bench_results/<run_name>/<program>.json    (default run_name = '_latest')

Compare reports default to:
  <repo>/bench_results/_reports/<run_a>_vs_<run_b>.html
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

from scripts.commands import BUILD_DIR, OUTPUT_DIR, _REPO_ROOT, _is_windows
from scripts.commands.build import _auto_configure


# Discovered statically; matches the layout under bench/<program>/.
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
    return Path(f"{OUTPUT_DIR}/bin/{config}/bench/{_bench_target(program)}{exe_suffix}")


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
        cwd=_REPO_ROOT,
        check=False,
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
        help="Build and run benchmarks (Release by default).",
    )
    sub = parser.add_subparsers(dest="bench_cmd")

    run = sub.add_parser("run", help="Run a benchmark program (default).")
    run.add_argument("program", help="Bench program (e.g. debug_renderer)")
    run.add_argument("case", nargs="?", help="Run only this case (optional)")
    run.add_argument("-o", "--out", default="_latest",
                     help="Run name (relative path under bench_results/). Default: _latest")
    run.add_argument("--config", default="release", choices=["debug", "release"],
                     help="Build configuration. Default: release")
    run.add_argument("--build-only", action="store_true",
                     help="Build the bench binary, don't run it.")
    run.add_argument("--iters", type=int, default=None,
                     help="Override measured iteration count for all cases.")
    run.add_argument("--warmup", type=int, default=None,
                     help="Override warmup iteration count for all cases.")
    run.set_defaults(handler=_run)

    lst = sub.add_parser("list", help="List benchmark programs and registered cases.")
    lst.add_argument("program", nargs="?", help="Show cases of this program.")
    lst.add_argument("--config", default="release", choices=["debug", "release"])
    lst.set_defaults(handler=_list)

    cmp = sub.add_parser("compare", help="Compare two runs and emit an HTML report.")
    cmp.add_argument("run_a")
    cmp.add_argument("run_b")
    cmp.add_argument("-o", "--out", default=None,
                     help="HTML output path. Default: bench_results/_reports/<a>_vs_<b>.html")
    cmp.set_defaults(handler=_compare)

    # Top-level default: when no subcommand given but a positional arg looks
    # like a program, treat it as `bench run`.
    parser.set_defaults(handler=_dispatch)


def _dispatch(args) -> int:
    # Falls through if a subcommand was selected (handler is overwritten).
    print("Usage: gk bench <run|list|compare> ...   (try 'gk bench --help')")
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
        ["cmake", "--build", build_dir_rel, "--config", config, "--target", target],
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

    out_path = Path(_REPO_ROOT) / "bench_results" / args.out / f"{args.program}.json"
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

    print(f"Running {exe.name} -> {out_path.relative_to(_REPO_ROOT)}")
    return subprocess.run(cmd, cwd=str(Path(_REPO_ROOT) / "working_dir"), check=False).returncode


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
        ["cmake", "--build", build_dir_rel, "--config", config, "--target", target],
        cwd=_REPO_ROOT, check=False,
    ).returncode
    if rc != 0:
        return rc

    exe = _bench_executable(args.program, config)
    if not exe.exists():
        print(f"Bench binary not found: {exe}")
        return 1
    return subprocess.run([str(exe), "--list"], check=False).returncode


def _compare(args) -> int:
    bench_results = Path(_REPO_ROOT) / "bench_results"
    out = args.out or str(bench_results / "_reports" / f"{args.run_a.replace('/', '_')}_vs_{args.run_b.replace('/', '_')}.html")
    Path(out).parent.mkdir(parents=True, exist_ok=True)

    report_script = Path(_REPO_ROOT) / "scripts" / "bench_report.py"
    return subprocess.run(
        [sys.executable, str(report_script),
         str(bench_results / args.run_a),
         str(bench_results / args.run_b),
         "--out", out],
        cwd=_REPO_ROOT, check=False,
    ).returncode
