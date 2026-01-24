from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


def _write_if_missing(path: Path, content: str) -> None:
    if path.exists():
        return
    path.write_text(content, encoding="utf-8")


def _ensure_env_scripts(repo_root: Path) -> None:
    bash_env = repo_root / ".gecko_env"
    ps_env = repo_root / ".gecko_env.ps1"
    bat_env = repo_root / ".gecko_env.bat"

    _write_if_missing(
        bash_env,
        """#!/usr/bin/env bash
REPO_ROOT=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"
export PATH=\"${REPO_ROOT}/bin:${PATH}\"
""",
    )

    _write_if_missing(
        ps_env,
        """$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:PATH = \"$RepoRoot\\bin;$env:PATH\"
""",
    )

    _write_if_missing(
        bat_env,
        """@echo off
set \"REPO_ROOT=%~dp0\"
set \"PATH=%REPO_ROOT%bin;%PATH%\"
""",
    )
def _ensure_envrc(repo_root: Path) -> None:
    envrc = repo_root / ".envrc"
    _write_if_missing(envrc, "source ./.gecko_env\n")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    _ensure_env_scripts(repo_root)
    _ensure_envrc(repo_root)

    if shutil.which("direnv"):
        subprocess.run(["direnv", "allow", str(repo_root)], check=False)

    print("Gecko dev env files created.")
    print("\nBash/Zsh:")
    print("  source ./.gecko_env   (or use direnv if installed)")
    print("\nPowerShell:")
    print("  .\\.gecko_env.ps1")
    print("\nCmd.exe:")
    print("  .\\.gecko_env.bat")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
