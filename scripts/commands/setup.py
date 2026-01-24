from __future__ import annotations

from scripts import setup_dev


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "setup",
        help="Create repo-local PATH + completion setup",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    return setup_dev.main()
