from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

from scripts.commands import BUILD_DIR, OUTPUT_DIR, _REPO_ROOT, is_network_path, _is_windows
from scripts.commands.build import _auto_configure

# Unit test targets (headless, always run)
UNIT_TARGETS = ["core_tests", "platform_tests", "runtime_tests", "math_tests"]

# Feature test targets (need live display / real backends)
FEATURE_TARGETS = ["platform_feature_tests"]


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "test",
        help="Build and run tests",
    )
    parser.add_argument(
        "config",
        nargs="?",
        default="debug",
        choices=["debug", "release"],
        help="Test configuration",
    )
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="Only build tests, don't run them",
    )

    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--unit",
        action="store_true",
        help="Run only unit tests (default when no flag is given)",
    )
    group.add_argument(
        "--feature",
        action="store_true",
        help="Run only feature tests (require live display / real backends)",
    )
    group.add_argument(
        "--all",
        action="store_true",
        help="Run both unit and feature tests",
    )

    parser.set_defaults(handler=_run)


def _run(args) -> int:
    config = "Debug" if args.config == "debug" else "Release"

    # Decide which targets to build & run
    if args.all:
        targets = UNIT_TARGETS + FEATURE_TARGETS
    elif args.feature:
        targets = list(FEATURE_TARGETS)
    else:
        # Default: unit only
        targets = list(UNIT_TARGETS)

    # Ensure tests are enabled in the build configuration
    build_dir_rel = os.path.relpath(BUILD_DIR, _REPO_ROOT)
    cache_file = os.path.join(BUILD_DIR, "CMakeCache.txt")

    if not os.path.isfile(cache_file):
        rc = _auto_configure()
        if rc != 0:
            return rc

    tests_enabled = False
    if os.path.isfile(cache_file):
        with open(cache_file) as f:
            for line in f:
                if line.strip() == "GECKO_BUILD_TESTS:BOOL=ON":
                    tests_enabled = True
                    break

    if not tests_enabled:
        print("Enabling tests in build configuration...")
        enable_result = subprocess.run(
            ["cmake", "-B", build_dir_rel, "-DGECKO_BUILD_TESTS=ON"],
            cwd=_REPO_ROOT,
            check=False,
        )
        if enable_result.returncode != 0:
            return enable_result.returncode

    # Build selected test targets
    print(f"Building tests ({config})...")
    for target in targets:
        build_result = subprocess.run(
            ["cmake", "--build", build_dir_rel, "--config", config, "--target", target],
            cwd=_REPO_ROOT,
            check=False,
        )
        if build_result.returncode != 0:
            return build_result.returncode

    if args.build_only:
        print(f"\nTests built successfully in {OUTPUT_DIR}/bin/{config}/tests/")
        return 0

    # Run tests directly (Catch2 handles test discovery and reporting)
    print(f"\nRunning tests...")
    exe_suffix = ".exe" if _is_windows() else ""
    overall_result = 0

    # On Windows network shares, SmartScreen blocks unsigned executables in
    # non-interactive mode. Copy to a local temp dir so they run without prompts.
    # On local drives this is skipped — executables run directly.
    use_temp_copy = is_network_path(os.path.abspath(OUTPUT_DIR))

    if use_temp_copy:
        import shutil
        import tempfile

        with tempfile.TemporaryDirectory(prefix="gecko_tests_") as tmpdir:
            # Copy all DLLs from the output bin dir so tests can find them
            bin_dir = Path(f"{OUTPUT_DIR}/bin/{config}")
            for dll in bin_dir.glob("*.dll"):
                shutil.copy2(dll, Path(tmpdir) / dll.name)

            for test_target in targets:
                test_executable = Path(f"{OUTPUT_DIR}/bin/{config}/tests/{test_target}{exe_suffix}")
                if not test_executable.exists():
                    print(f"  Warning: {test_executable} not found, skipping")
                    continue
                local_exe = Path(tmpdir) / test_executable.name
                shutil.copy2(test_executable, local_exe)
                print(f"\n--- {test_target} ---")
                test_result = subprocess.run([str(local_exe)], check=False)
                if test_result.returncode != 0:
                    print(f"  {test_target} exited with code {test_result.returncode}")
                    overall_result = test_result.returncode
    else:
        for test_target in targets:
            test_executable = Path(f"{OUTPUT_DIR}/bin/{config}/tests/{test_target}{exe_suffix}")
            if not test_executable.exists():
                print(f"  Warning: {test_executable} not found, skipping")
                continue
            print(f"\n--- {test_target} ---")
            test_result = subprocess.run([str(test_executable)], check=False)
            if test_result.returncode != 0:
                overall_result = test_result.returncode

    return overall_result
