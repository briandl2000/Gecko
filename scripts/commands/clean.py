from __future__ import annotations

import os
import stat
import shutil

from scripts.commands import BUILD_DIR, OUTPUT_DIR, PLATFORM_ID


def _force_remove_readonly(func, path, exc_info):
    """Handle read-only files (e.g. git pack files) during rmtree."""
    os.chmod(path, stat.S_IWRITE)
    func(path)


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "clean",
        help="Remove build artifacts",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Remove all platform build dirs, not just the current one",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    if args.all:
        for d in ["out"]:
            path = os.path.join(os.getcwd(), d)
            if os.path.isdir(path):
                print(f"Removing {path}")
                shutil.rmtree(path, onexc=_force_remove_readonly)
    else:
        for path in [BUILD_DIR, OUTPUT_DIR]:
            if os.path.isdir(path):
                print(f"Removing {path}")
                shutil.rmtree(path, onexc=_force_remove_readonly)

    # Also clean local debug copy on Windows
    if os.name == "nt":
        debug_dir = os.path.join("C:\\", "Gecko", "debug")
        if os.path.isdir(debug_dir):
            print(f"Removing {debug_dir}")
            shutil.rmtree(debug_dir, onexc=_force_remove_readonly)

    print("Clean complete. Run setup script to reconfigure.")
    return 0
