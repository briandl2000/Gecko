from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

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

    # Enable tests via cache variable (fast, no reconfigure)
    enable_tests = subprocess.run(
        ["cmake", "-B", "out/build", "-DGECKO_BUILD_TESTS=ON"],
        check=False,
        capture_output=True,
    )
    if enable_tests.returncode != 0:
        print(enable_tests.stderr.decode(), file=sys.stderr)
        return enable_tests.returncode

    # Build selected test targets
    print(f"Building tests ({config})...")
    for target in targets:
        build_result = subprocess.run(
            ["cmake", "--build", "out/build", "--config", config, "--target", target],
            check=False,
        )
        if build_result.returncode != 0:
            return build_result.returncode

    if args.build_only:
        print(f"\nTests built successfully in out/bin/{config}/tests/")
        return 0

    # Run tests directly (Catch2 handles test discovery and reporting)
    print(f"\nRunning tests...")
    exe_suffix = ".exe" if os.name == "nt" else ""
    overall_result = 0
    for test_target in targets:
        test_executable = Path(f"out/bin/{config}/tests/{test_target}{exe_suffix}")
        if not test_executable.exists():
            print(f"  Warning: {test_executable} not found, skipping")
            continue
        test_result = subprocess.run(
            [str(test_executable)],
            check=False,
        )
        if test_result.returncode != 0:
            overall_result = test_result.returncode

    return overall_result
