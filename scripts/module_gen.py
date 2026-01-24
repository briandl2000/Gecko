import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("name", help="Module name (required)")
    parser.add_argument(
        "--dep",
        action="append",
        default=[],
        metavar="DEP",
        help="Add a dependency (repeatable) Example: --dep core --dep platform",
    )

    args = parser.parse_args()

    print("name", args.name)
    print("deps", args.dep)


if __name__ == "__main__":
    main()
