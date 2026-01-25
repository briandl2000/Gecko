#!/usr/bin/env python3

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


def gather_files(root: Path) -> list[Path]:
    targets: list[Path] = []
    for folder in ("src", "include", "examples"):
        base = root / folder
        if not base.exists():
            continue
        for ext in ("*.cpp", "*.h"):
            targets.extend(base.rglob(ext))
    return sorted({path.resolve() for path in targets})


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    clang_format = shutil.which("clang-format")
    if not clang_format:
        print("clang-format not found in PATH.", file=sys.stderr)
        return 1

    files = gather_files(root)
    if not files:
        print("No C++ files found to format.")
        return 0

    print(f"Formatting {len(files)} files...")
    for path in files:
        result = subprocess.run([clang_format, "-i", str(path)], check=False)
        if result.returncode != 0:
            return result.returncode

    print("Done! All files formatted.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
