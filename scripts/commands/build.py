from __future__ import annotations

import os
import shutil
import subprocess
import time

from scripts.commands import BUILD_DIR, _REPO_ROOT, _is_windows


_CONFIGS = {
    "debug": "Debug",
    "release": "Release",
}

# Test targets are EXCLUDE_FROM_ALL in CMake to keep `gk build` fast for the
# day-to-day edit/build/run loop. They are only built when the user runs
# `gk build tests` or `gk test`. Keep this in sync with
# scripts/commands/test.py.
_TEST_TARGETS = (
    "core_tests",
    "platform_tests",
    "runtime_tests",
    "math_tests",
    "graphics_tests",
    "platform_feature_tests",
    "runtime_feature_tests",
    "graphics_feature_tests",
)


def _auto_configure() -> int:
    """Auto-configure CMake when the build directory doesn't exist yet."""
    env = os.environ.copy()

    # Let CMake auto-detect the compiler. Honour CC/CXX if the caller
    # already set them (or a toolchain file is in use); otherwise CMake
    # picks whatever is on PATH (gcc on Linux, MSVC on Windows, the
    # cross compiler when invoked via gk-pi's toolchain file).
    toolchain = env.get("CMAKE_TOOLCHAIN_FILE")
    # Treat both unset and set-but-empty CC/CXX as "not configured" so an
    # accidental `export CXX=` doesn't bypass the compiler-presence check
    # and surface a less actionable CMake error later.
    if not toolchain and not env.get("CXX") and not env.get("CC"):
        if not (shutil.which("c++") or shutil.which("g++")
                or shutil.which("clang++") or shutil.which("cl")):
            print("ERROR: No C++ compiler found on PATH.")
            if _is_windows():
                print("  Install Visual Studio Build Tools or use MSYS2.")
            else:
                print("  Install build-essential / gcc-c++ / base-devel.")
            return 1

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
        "what",
        nargs="?",
        default="debug",
        help="What to build: debug | release | all | tests",
    )
    parser.add_argument(
        "config",
        nargs="?",
        default=None,
        help="Config when 'what' is 'tests' (debug | release, default debug)",
    )
    parser.set_defaults(handler=_run)


def _build(build_dir_rel: str, cmake_config: str,
           targets: tuple[str, ...] = ()) -> int:
    cmd = ["cmake", "--build", build_dir_rel, "--config", cmake_config]
    if targets:
        cmd += ["--target", *targets]
    return subprocess.run(cmd, cwd=_REPO_ROOT, check=False).returncode


def _run(args) -> int:
    start = time.perf_counter()

    if (not os.path.isdir(BUILD_DIR)
            or not os.path.isfile(os.path.join(BUILD_DIR, "CMakeCache.txt"))):
        rc = _auto_configure()
        if rc != 0:
            return rc

    # Use a path relative to the repo root for cmake --build so that on Windows
    # mapped drives, cmake/ninja don't resolve to UNC paths (which cmd.exe
    # cannot use as a working directory).
    build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)

    what = args.what.lower()

    # `gk build tests [config]` -> build only the test targets.
    if what == "tests":
        cfg = (args.config or "debug").lower()
        if cfg not in _CONFIGS:
            print(f"Unknown test config: {cfg!r} (expected debug or release)")
            return 2
        rc = _build(build_dir_rel, _CONFIGS[cfg], _TEST_TARGETS)
        _report(start, rc)
        return rc

    # `gk build all` -> both configs, engine + examples only.
    if what == "all":
        rc = _build(build_dir_rel, "Debug")
        if rc == 0:
            rc = _build(build_dir_rel, "Release")
        _report(start, rc)
        return rc

    if what not in _CONFIGS:
        print(f"Unknown build target: {what!r} "
              f"(expected debug, release, all, or tests)")
        return 2

    rc = _build(build_dir_rel, _CONFIGS[what])
    _report(start, rc)
    return rc


def _report(start: float, rc: int) -> None:
    elapsed = time.perf_counter() - start
    status = "ok" if rc == 0 else f"FAILED ({rc})"
    print(f"[gk build] {status} in {elapsed:.2f}s")
