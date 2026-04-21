from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

from scripts.commands import BUILD_DIR, _REPO_ROOT, _is_windows


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "setup",
        help="Configure CMake build system",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove existing build and reconfigure",
    )
    parser.set_defaults(handler=_run)


def _check_tools() -> bool:
    """Verify required build tools are available."""
    ok = True

    if not shutil.which("cmake"):
        print("ERROR: cmake not found. Install cmake first.", file=sys.stderr)
        ok = False

    if not shutil.which("ninja"):
        print("ERROR: ninja not found.", file=sys.stderr)
        if not _is_windows():
            print("  Install with: sudo apt-get install ninja-build")
        ok = False

    if not shutil.which("gcc") or not shutil.which("g++"):
        print("ERROR: gcc/g++ not found.", file=sys.stderr)
        if _is_windows():
            print("  Use the MSYS2 UCRT64 shell (gcc is installed there)")
        else:
            print("  Install with: sudo apt-get install gcc g++")
        ok = False

    return ok


def _setup_env() -> dict[str, str]:
    """Return environment with correct compiler settings."""
    env = os.environ.copy()

    if not _is_windows():
        # On Linux/macOS, ensure GCC is used
        env.setdefault("CC", "gcc")
        env.setdefault("CXX", "g++")

    return env


def _run(args) -> int:
    repo_root = Path(_REPO_ROOT)

    if not _check_tools():
        return 1

    if args.clean and os.path.isdir(BUILD_DIR):
        print("Cleaning build directory...")
        shutil.rmtree(os.path.join(_REPO_ROOT, "out"))

    env = _setup_env()

    print("Configuring CMake...")
    result = subprocess.run(
        ["cmake", "--preset", "debug"],
        cwd=_REPO_ROOT,
        env=env,
        check=False,
    )

    # Point git to our hooks directory
    hooks_dir = repo_root / ".githooks"
    if hooks_dir.is_dir():
        subprocess.run(
            ["git", "config", "core.hooksPath", ".githooks"],
            cwd=_REPO_ROOT,
            check=False,
            capture_output=True,
        )
        print("Git hooks configured (.githooks/)")

    if result.returncode == 0:
        print("\nSetup complete! To use gecko commands:")
        print("  source scripts/setup.sh    # Linux/macOS/MSYS2")

    return result.returncode
