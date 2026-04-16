from __future__ import annotations

import os
import stat
import shutil
import sys

from scripts.commands import BUILD_DIR, OUTPUT_DIR, _REPO_ROOT


def _force_remove_readonly(func, path, exc_info):
    """Handle read-only files (e.g. git pack files) during rmtree."""
    os.chmod(path, stat.S_IWRITE)
    func(path)


def _rmtree(path):
    """Remove directory tree, handling read-only files on all Python versions."""
    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=_force_remove_readonly)
    else:
        shutil.rmtree(path, onerror=_force_remove_readonly)


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
        out_dir = os.path.join(_REPO_ROOT, "out")
        if os.path.isdir(out_dir):
            print(f"Removing {out_dir}")
            _rmtree(out_dir)
    else:
        for path in [BUILD_DIR, OUTPUT_DIR]:
            if os.path.isdir(path):
                print(f"Removing {path}")
                _rmtree(path)

    # Also clean local debug copy on Windows
    if os.name == "nt":
        debug_dir = os.path.join("C:\\", "Gecko", "debug")
        if os.path.isdir(debug_dir):
            print(f"Removing {debug_dir}")
            _rmtree(debug_dir)

    print("Clean complete. Run setup script to reconfigure.")
    return 0
