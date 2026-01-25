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
        configs = ["debug", "release"]
    else:
        configs = [args.config]

    for config in configs:
        cmake_config = _CONFIGS[config]
        result = subprocess.run(
            ["cmake", "--build", "build", "--config", cmake_config],
            check=False,
        )
        if result.returncode != 0:
            return result.returncode

    return 0
