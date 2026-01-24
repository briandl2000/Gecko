from __future__ import annotations

from scripts import example_gen


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "new_example",
        help="Create a new example project",
    )
    parser.add_argument("name", help="Example name (lowercase, underscores)")
    parser.add_argument(
        "--dep",
        action="append",
        default=[],
        metavar="DEP",
        help="Add a dependency (repeatable) Example: --dep platform",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    argv = [args.name]
    for dep in args.dep:
        argv.extend(["--dep", dep])
    return example_gen.main(argv)
