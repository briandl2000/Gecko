from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

from scripts.commands import OUTPUT_DIR, is_network_path, _is_windows


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "run",
        help="Run a built target (optionally build it first)",
    )
    parser.add_argument(
        "target",
        nargs="?",
        help=("Target name to run (e.g. core_example). "
              "Omit or pass '@cached' to use the cached target "
              "from .vscode/run_target (see `gk target`)."),
    )
    parser.add_argument(
        "config",
        nargs="?",
        default="debug",
        choices=["debug", "release"],
        help="Build configuration",
    )
    parser.add_argument(
        "--build", "-b",
        action="store_true",
        help="Build the target (incremental) before running it.",
    )
    parser.add_argument(
        "extra_args",
        nargs="*",
        help="Extra arguments to pass to the target",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    if not args.target or args.target == "@cached":
        from scripts.commands.target import read_cached_target
        args.target = read_cached_target()
        print(f"[gk run] using cached target: {args.target}")

    config = "Debug" if args.config == "debug" else "Release"
    exe_suffix = ".exe" if _is_windows() else ""
    executable = Path(f"{OUTPUT_DIR}/bin/{config}/{args.target}{exe_suffix}")

    if args.build:
        from scripts.commands import BUILD_DIR, _REPO_ROOT
        from scripts.commands.build import _auto_configure

        if (not os.path.isdir(BUILD_DIR)
                or not os.path.isfile(os.path.join(BUILD_DIR, "CMakeCache.txt"))):
            rc = _auto_configure()
            if rc != 0:
                return rc

        build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)
        t0 = time.perf_counter()
        # Build only the requested target -- ninja still picks up its
        # transitive dependencies but skips unrelated examples/tests.
        rc = subprocess.run(
            ["cmake", "--build", build_dir_rel,
             "--config", config, "--target", args.target],
            cwd=_REPO_ROOT, check=False,
        ).returncode
        elapsed = time.perf_counter() - t0
        if rc != 0:
            print(f"[gk run] build FAILED ({rc}) in {elapsed:.2f}s")
            return rc
        print(f"[gk run] built {args.target} ({config}) in {elapsed:.2f}s")

    if not executable.exists():
        print(f"Error: {executable} not found")
        if not args.build:
            print(f"Hint: pass --build (or run 'gk build {args.config}') first.")
        return 1

    working_dir = Path("working_dir")
    working_dir.mkdir(exist_ok=True)

    cmd = [str(executable)] + args.extra_args

    # On Windows network shares, SmartScreen blocks unsigned executables.
    # Copy to a local temp dir with required DLLs to bypass this.
    if is_network_path(str(executable.resolve())):
        import shutil
        import tempfile

        with tempfile.TemporaryDirectory(prefix="gecko_run_") as tmpdir:
            local_exe = Path(tmpdir) / executable.name
            shutil.copy2(executable, local_exe)

            for dll in executable.parent.glob("*.dll"):
                shutil.copy2(dll, Path(tmpdir) / dll.name)

            cmd = [str(local_exe)] + args.extra_args
            result = subprocess.run(cmd, cwd=str(working_dir.resolve()), check=False)
            return result.returncode
    else:
        result = subprocess.run(cmd, cwd=str(working_dir.resolve()), check=False)
        return result.returncode
