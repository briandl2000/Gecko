from __future__ import annotations

import subprocess
import sys
from pathlib import Path


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
    repo_root = Path(__file__).resolve().parents[2]
    build_dir = repo_root / "out" / "build"
    
    if args.clean and build_dir.exists():
        import shutil
        print("Cleaning build directory...")
        shutil.rmtree(repo_root / "out")
    
    print("Configuring CMake...")
    result = subprocess.run(
        ["cmake", "--preset", "debug"],
        cwd=repo_root,
        check=False,
    )
    
    if result.returncode == 0:
        print("\nSetup complete! To use gecko commands:")
        print("  Linux/macOS: source scripts/setup.sh")
        print("  PowerShell:  . .\\scripts\\setup.ps1")
    
    return result.returncode
