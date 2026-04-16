from __future__ import annotations

import subprocess
import sys
from scripts.commands import BUILD_DIR, _REPO_ROOT


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


def _run(args) -> int:
    import os
    from pathlib import Path

    repo_root = Path(_REPO_ROOT)

    if args.clean and os.path.isdir(BUILD_DIR):
        import shutil
        print("Cleaning build directory...")
        shutil.rmtree(os.path.join(_REPO_ROOT, "out"))

    print("Configuring CMake...")
    result = subprocess.run(
        ["cmake", "--preset", "debug"],
        cwd=_REPO_ROOT,
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
        print("  Linux/macOS: source scripts/setup.sh")
        print("  PowerShell:  . .\\scripts\\setup.ps1")
    
    return result.returncode
