from __future__ import annotations

import subprocess


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
    if args.config == "all":
        # Build both configurations in parallel using multi-config generator
        result = subprocess.run(
            ["cmake", "--build", "out/build", "--config", "Debug"],
            check=False,
        )
        if result.returncode != 0:
            return result.returncode
        
        result = subprocess.run(
            ["cmake", "--build", "out/build", "--config", "Release"],
            check=False,
        )
        return result.returncode
    else:
        cmake_config = _CONFIGS[args.config]
        result = subprocess.run(
            ["cmake", "--build", "out/build", "--config", cmake_config],
            check=False,
        )
        return result.returncode
