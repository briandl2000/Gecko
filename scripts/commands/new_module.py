from __future__ import annotations

from scripts import module_gen


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "new_module",
        help="Create a new engine module",
    )
    parser.add_argument("name", help="Module name (lowercase, underscores)")
    parser.add_argument(
        "--dep",
        action="append",
        default=[],
        metavar="DEP",
        help="Add a dependency (repeatable) Example: --dep core --dep platform",
    )
    parser.set_defaults(handler=_run)


def _run(args) -> int:
    argv = [args.name]
    for dep in args.dep:
        argv.extend(["--dep", dep])
    return module_gen.main(argv)
