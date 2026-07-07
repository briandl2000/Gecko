"""`gk target` -- get/set the cached run/debug target.

The cached target name is persisted at `.vscode/run_target` so editor
tasks and launch configs can resolve it without re-prompting.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

from scripts.commands import OUTPUT_DIR, _REPO_ROOT, _is_windows

DEFAULT_TARGET = "graphics_example"


def _cache_file() -> Path:
    return Path(_REPO_ROOT) / ".vscode" / "run_target"


def read_cached_target() -> str:
    f = _cache_file()
    if f.exists():
        name = f.read_text(encoding="utf-8").strip()
        if name:
            return name
    return DEFAULT_TARGET


def write_cached_target(name: str) -> None:
    f = _cache_file()
    f.parent.mkdir(parents=True, exist_ok=True)
    f.write_text(name + "\n", encoding="utf-8")


def refresh_symlink(target: str, config: str, link_dir: str = ".vscode") -> Path:
    """Refresh the cached launch symlink to point at the exe.

    Returns the symlink path.  Editor launch configs use this for `program`,
    so they never need to know which target was picked.
    """
    cfg_dir = "Debug" if config == "debug" else "Release"
    exe_suffix = ".exe" if _is_windows() else ""
    exe = Path(OUTPUT_DIR) / "bin" / cfg_dir / f"{target}{exe_suffix}"

    link = Path(_REPO_ROOT) / link_dir / f"cached_{config}_bin{exe_suffix}"
    link.parent.mkdir(parents=True, exist_ok=True)

    # Symlinks aren't reliable on Windows without admin -- write a copy there.
    if _is_windows():
        if exe.exists():
            import shutil
            shutil.copy2(exe, link)
    else:
        if link.is_symlink() or link.exists():
            link.unlink()
        # Use absolute target so gdb resolves it regardless of cwd.
        os.symlink(exe.resolve(), link)
    return link


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "target",
        help="Get or set the cached run/debug target",
    )
    parser.add_argument(
        "name",
        nargs="?",
        help="Target name to cache (omit to print current)",
    )
    parser.set_defaults(handler=_handle)


def _handle(args) -> int:
    if args.name is None:
        print(read_cached_target())
        return 0
    write_cached_target(args.name)
    print(f"[gk target] cached -> {args.name}", file=sys.stderr)
    return 0
