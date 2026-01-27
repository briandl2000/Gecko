from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


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
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    config = "Debug" if args.config == "debug" else "Release"
    build_dir = Path("out/build")
    
    # Enable tests via cache variable (fast, no reconfigure)
    enable_tests = subprocess.run(
        ["cmake", "-B", "out/build", "-DGECKO_BUILD_TESTS=ON"],
        check=False,
        capture_output=True,
    )
    if enable_tests.returncode != 0:
        print(enable_tests.stderr.decode(), file=sys.stderr)
        return enable_tests.returncode
    
    # Build tests only (not everything)
    print(f"Building tests ({config})...")
    build_result = subprocess.run(
        ["cmake", "--build", "out/build", "--config", config, "--target", "math_tests"],
        check=False,
    )
    if build_result.returncode != 0:
        return build_result.returncode
    
    if args.build_only:
        print(f"\nTests built successfully in out/bin/{config}/tests/")
        return 0
    
    # Run tests directly (Catch2 handles test discovery and reporting)
    print(f"\nRunning tests...")
    test_executable = f"out/bin/{config}/tests/math_tests"
    test_result = subprocess.run(
        [test_executable],
        check=False,
    )
    
    return test_result.returncode
