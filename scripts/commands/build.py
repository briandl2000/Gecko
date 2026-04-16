from __future__ import annotations

import os
import subprocess

from scripts.commands import BUILD_DIR


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

    if args.config == "all":
        # Build both configurations in parallel using multi-config generator
        result = subprocess.run(
            ["cmake", "--build", BUILD_DIR, "--config", "Debug"],
            check=False,
        )
        if result.returncode != 0:
            return result.returncode
        
        result = subprocess.run(
            ["cmake", "--build", BUILD_DIR, "--config", "Release"],
            check=False,
        )
        return result.returncode
    else:
        cmake_config = _CONFIGS[args.config]
        result = subprocess.run(
            ["cmake", "--build", BUILD_DIR, "--config", cmake_config],
            check=False,
        )
        return result.returncode
