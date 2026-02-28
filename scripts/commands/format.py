from __future__ import annotations

from scripts import format as format_script


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "format",
        help="Format all C++ source files",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    return format_script.main()
