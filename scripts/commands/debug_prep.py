"""`gk debug-prep` -- preLaunchTask helper for VS Code cached debug config.

Reads the cached target, builds it, and refreshes the
`.vscode/cached_<config>_bin` symlink so launch.json's static `program`
path resolves to the chosen exe.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time

from scripts.commands import BUILD_DIR, _REPO_ROOT


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "debug-prep",
        help="Build the cached target and refresh the launch symlink",
    )
    parser.add_argument(
        "config",
        choices=["debug", "release"],
        default="debug",
        nargs="?",
    )
    parser.set_defaults(handler=_handle)


def _handle(args) -> int:
    from scripts.commands.target import read_cached_target, refresh_symlink
    from scripts.commands.build import _auto_configure

    target = read_cached_target()
    cmake_cfg = "Debug" if args.config == "debug" else "Release"

    if (not os.path.isdir(BUILD_DIR)
            or not os.path.isfile(os.path.join(BUILD_DIR, "CMakeCache.txt"))):
        rc = _auto_configure()
        if rc != 0:
            return rc

    build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)
    t0 = time.perf_counter()
    rc = subprocess.run(
        ["cmake", "--build", build_dir_rel,
         "--config", cmake_cfg, "--target", target],
        cwd=_REPO_ROOT, check=False,
    ).returncode
    elapsed = time.perf_counter() - t0
    if rc != 0:
        print(f"[gk debug-prep] build FAILED ({rc}) in {elapsed:.2f}s")
        return rc
    print(f"[gk debug-prep] built {target} ({cmake_cfg}) in {elapsed:.2f}s")

    link = refresh_symlink(target, args.config)
    print(f"[gk debug-prep] launch program -> {link}")
    return 0
