from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

from scripts.commands import OUTPUT_DIR, is_network_path, _is_windows


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "run",
        help="Run a built target",
    )
    parser.add_argument(
        "target",
        help="Target name to run (e.g. core_example, platform_example)",
    )
    parser.add_argument(
        "config",
        nargs="?",
        default="debug",
        choices=["debug", "release"],
        help="Build configuration",
    )
    parser.add_argument(
        "extra_args",
        nargs="*",
        help="Extra arguments to pass to the target",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    config = "Debug" if args.config == "debug" else "Release"
    exe_suffix = ".exe" if _is_windows() else ""
    executable = Path(f"{OUTPUT_DIR}/bin/{config}/{args.target}{exe_suffix}")

    if not executable.exists():
        print(f"Error: {executable} not found")
        print(f"Run 'gk build {args.config}' first.")
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
