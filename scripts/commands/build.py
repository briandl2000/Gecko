from __future__ import annotations

import os
import shutil
import subprocess

from scripts.commands import BUILD_DIR, _REPO_ROOT, _is_windows


_CONFIGS = {
    "debug": "Debug",
    "release": "Release",
}

# Test targets are EXCLUDE_FROM_ALL in CMake (to keep `cmake --build` fast for
# development); when GECKO_BUILD_TESTS=ON we still want `gk build` to build
# them so cross-compile flows (gk-pi, Windows mirror) get test binaries
# alongside the engine and examples. Keep this in sync with
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


def _tests_enabled() -> bool:
    cache_file = os.path.join(BUILD_DIR, "CMakeCache.txt")
    if not os.path.isfile(cache_file):
        return False
    try:
        with open(cache_file) as f:
            for line in f:
                if line.strip() == "GECKO_BUILD_TESTS:BOOL=ON":
                    return True
    except OSError:
        return False
    return False


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

        if _tests_enabled():
            result = subprocess.run(
                ["cmake", "--build", build_dir_rel, "--config", "Debug",
                 "--target", *_TEST_TARGETS],
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
        if result.returncode != 0:
            return result.returncode

        if _tests_enabled():
            result = subprocess.run(
                ["cmake", "--build", build_dir_rel, "--config", "Release",
                 "--target", *_TEST_TARGETS],
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
        if result.returncode != 0:
            return result.returncode

        if _tests_enabled():
            result = subprocess.run(
                ["cmake", "--build", build_dir_rel, "--config", cmake_config,
                 "--target", *_TEST_TARGETS],
                cwd=_REPO_ROOT,
                check=False,
            )
        return result.returncode
