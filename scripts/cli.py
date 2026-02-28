from __future__ import annotations

import argparse
import importlib
import pkgutil
import sys
from pathlib import Path
from types import ModuleType


def _ensure_repo_on_path() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    repo_str = str(repo_root)
    if repo_str not in sys.path:
        sys.path.insert(0, repo_str)


def _iter_command_modules() -> list[ModuleType]:
    modules: list[ModuleType] = []
    package_name = "scripts.commands"
    _ensure_repo_on_path()
    package = importlib.import_module(package_name)
    for mod in pkgutil.iter_modules(package.__path__, package_name + "."):
        modules.append(importlib.import_module(mod.name))
    return modules


def _register_commands(subparsers) -> None:
    for module in _iter_command_modules():
        register = getattr(module, "register", None)
        if register is None:
            continue
        register(subparsers)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="gecko")
    subparsers = parser.add_subparsers(dest="command")

    _register_commands(subparsers)

    args = parser.parse_args(argv)
    handler = getattr(args, "handler", None)
    if handler is None:
        parser.print_help()
        return 1

    return int(handler(args))


if __name__ == "__main__":
    raise SystemExit(main())
