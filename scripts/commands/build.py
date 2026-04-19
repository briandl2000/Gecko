from __future__ import annotations

import os
import shutil
import subprocess

from scripts.commands import BUILD_DIR, _REPO_ROOT


_CONFIGS = {
    "debug": "Debug",
    "release": "Release",
}


def _auto_configure() -> int:
    """Auto-configure CMake when the build directory doesn't exist yet."""
    env = os.environ.copy()

    # Gecko uses GCC. When no toolchain file is in play (native builds),
    # make sure CMake picks up gcc instead of whatever cc/c++ defaults to.
    toolchain = env.get("CMAKE_TOOLCHAIN_FILE")
    if not toolchain:
        gcc = shutil.which("gcc")
        gpp = shutil.which("g++")
        if not gcc or not gpp:
            print("ERROR: GCC compiler not found. Gecko requires GCC.")
            return 1
        env.setdefault("CC", gcc)
        env.setdefault("CXX", gpp)

    cmake_args = [
        "cmake", "-S", _REPO_ROOT, "-B", BUILD_DIR,
        "-G", "Ninja Multi-Config",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DGECKO_BUILD_TESTS=ON",
    ]

    if toolchain:
        cmake_args.extend(["--toolchain", toolchain])

    print(f"Configuring build directory: {BUILD_DIR}")
    result = subprocess.run(cmake_args, env=env, check=False)
    return result.returncode


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
        rc = _auto_configure()
        if rc != 0:
            return rc

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
