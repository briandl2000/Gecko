from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


def _configure_cmake(repo_root: Path) -> int:
    """Configure CMake if not already done."""
    build_dir = repo_root / "out" / "build"
    
    if (build_dir / "CMakeCache.txt").exists():
        print("CMake already configured.")
        return 0
    
    print("Configuring CMake...")
    result = subprocess.run(
        ["cmake", "--preset", "debug"],
        cwd=repo_root,
        check=False,
    )
    return result.returncode


def _setup_compile_commands(repo_root: Path) -> None:
    """Create symlink for compile_commands.json in repo root."""
    source = repo_root / "out" / "build" / "compile_commands.json"
    target = repo_root / "compile_commands.json"
    
    if target.exists() or target.is_symlink():
        return
    
    if source.exists():
        try:
            target.symlink_to(source)
            print("Created compile_commands.json symlink")
        except OSError:
            # Windows without symlink permission - copy instead
            import shutil
            shutil.copy2(source, target)
            print("Copied compile_commands.json (symlink not available)")


def _print_usage() -> None:
    print("\n" + "=" * 50)
    print("Gecko development environment ready!")
    print("=" * 50)
    print("\nAdd bin/ to your PATH:")
    print("  export PATH=\"$PWD/bin:$PATH\"   # bash/zsh")
    print("  $env:PATH = \"$PWD\\bin;$env:PATH\"   # PowerShell")
    print("\nCommon commands:")
    print("  gecko build [debug|release|all]")
    print("  gecko test [debug|release]")
    print("  gecko format")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    os.chdir(repo_root)
    
    result = _configure_cmake(repo_root)
    if result != 0:
        return result
    
    _setup_compile_commands(repo_root)
    _print_usage()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
