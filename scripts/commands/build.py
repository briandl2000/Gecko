from __future__ import annotations

import os
import subprocess

from scripts.commands import BUILD_DIR, _REPO_ROOT


_CONFIGS = {
    "debug": "Debug",
    "release": "Release",
}


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "build",
        help="Build the engine (Debug/Release)",
    )
    parser.add_argument(
        "config",
        nargs="?",
        default="debug",
        choices=["debug", "release", "all"],
        help="Build configuration",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    if not os.path.isdir(BUILD_DIR) or not os.path.isfile(os.path.join(BUILD_DIR, "CMakeCache.txt")):
        print(f"Build directory not configured: {BUILD_DIR}")
        print("Run the setup script first:")
        print("  Linux/macOS: source scripts/setup.sh")
        print("  Windows:     . .\\scripts\\setup.ps1")
        return 1

    # Use a path relative to the repo root for cmake --build so that on Windows
    # mapped drives, cmake/ninja don't resolve to UNC paths (which cmd.exe
    # cannot use as a working directory).
    build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)

    if args.config == "all":
        result = subprocess.run(
            ["cmake", "--build", build_dir_rel, "--config", "Debug"],
            cwd=_REPO_ROOT,
            check=False,
        )
        if result.returncode != 0:
            return result.returncode

        result = subprocess.run(
            ["cmake", "--build", build_dir_rel, "--config", "Release"],
            cwd=_REPO_ROOT,
            check=False,
        )
        return result.returncode
    else:
        cmake_config = _CONFIGS[args.config]
        result = subprocess.run(
            ["cmake", "--build", build_dir_rel, "--config", cmake_config],
            cwd=_REPO_ROOT,
            check=False,
        )
        return result.returncode
