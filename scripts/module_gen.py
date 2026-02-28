import argparse
import re
import sys
from pathlib import Path


def to_pascal(name: str) -> str:
    parts = re.split(r"[_\-\s]+", name)
    return "".join(part[:1].upper() + part[1:] for part in parts if part)


def normalize_deps(deps: list[str]) -> list[str]:
    normalized = []
    for dep in deps:
        dep_name = dep.strip()
        if not dep_name:
            continue
        normalized.append(dep_name)
    return normalized


def module_target_name(module_name: str) -> str:
    return to_pascal(module_name)


def dep_to_target(dep: str) -> str:
    dep_name = dep.strip()
    if dep_name.startswith("Gecko::"):
        return dep_name
    return f"Gecko::{to_pascal(dep_name)}"


def render_dep_lines(deps: list[str]) -> str:
    targets = []
    for dep in deps:
        dep_lower = dep.lower()
        if dep_lower in {"core", "coreservices"}:
            continue
        target = dep_to_target(dep)
        if target not in targets:
            targets.append(target)
    if not targets:
        return ""
    return "\n    " + "\n    ".join(targets)


def ensure_template_exists(template_root: Path) -> bool:
    if not template_root.exists() or not template_root.is_dir():
        print(f"Template path missing: {template_root}", file=sys.stderr)
        return False
    return True


def ensure_repo_root(repo_root: Path) -> bool:
    required = ["CMakeLists.txt", "src", "include", "examples", "scripts"]
    missing = [name for name in required if not (repo_root / name).exists()]
    if missing:
        print(
            "Repo root sanity check failed. Missing: " + ", ".join(missing),
            file=sys.stderr,
        )
        return False
    return True


def copy_template(
    template_root: Path,
    repo_root: Path,
    module_name: str,
    module_pascal: str,
    dep_lines: str,
) -> bool:
    replacements = {
        "__MODULE__": module_name,
        "__MODULE_CAMEL__": module_pascal,
        "__MODULE_DEP_TARGETS__": dep_lines,
    }

    existing_files: list[Path] = []
    for path in template_root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(template_root)
        rel_str = str(rel).replace("__MODULE__", module_name)
        dest = repo_root / rel_str
        if dest.exists():
            existing_files.append(dest)

    if existing_files:
        print("Refusing to overwrite existing files:", file=sys.stderr)
        for path in existing_files:
            print(f"  {path}", file=sys.stderr)
        return False

    for path in template_root.rglob("*"):
        rel = path.relative_to(template_root)
        rel_str = str(rel).replace("__MODULE__", module_name)
        dest = repo_root / rel_str

        if path.is_dir():
            dest.mkdir(parents=True, exist_ok=True)
            continue

        content = path.read_text(encoding="utf-8")
        for token, value in replacements.items():
            content = content.replace(token, value)

        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(content, encoding="utf-8")

    return True


def update_src_cmake(src_cmake: Path, module_name: str) -> None:
    add_line = f"add_subdirectory({module_name})"
    content = src_cmake.read_text(encoding="utf-8")
    if add_line in content:
        return
    content = content.rstrip() + "\n" + add_line + "\n"
    src_cmake.write_text(content, encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("name", help="Module name (required)")
    parser.add_argument(
        "--dep",
        action="append",
        default=[],
        metavar="DEP",
        help="Add a dependency (repeatable) Example: --dep core --dep platform",
    )

    args = parser.parse_args(argv)

    raw_name = args.name.strip()
    if not raw_name:
        print("Module name is required.", file=sys.stderr)
        return 1

    module_name = raw_name.lower()
    if not re.fullmatch(r"[a-z][a-z0-9_]*", module_name):
        print(
            "Module name must match [a-z][a-z0-9_]* (lowercase, underscores).",
            file=sys.stderr,
        )
        return 1

    module_pascal = module_target_name(module_name)
    deps = normalize_deps(args.dep)

    repo_root = Path(__file__).resolve().parents[1]
    if not ensure_repo_root(repo_root):
        return 1

    template_root = repo_root / "template" / "module"
    if not ensure_template_exists(template_root):
        return 1

    src_dir = repo_root / "src" / module_name
    include_dir = repo_root / "include" / "gecko" / module_name
    if src_dir.exists() or include_dir.exists():
        print("Module already exists.", file=sys.stderr)
        return 1

    dep_lines = render_dep_lines(deps)

    if not copy_template(template_root, repo_root, module_name, module_pascal, dep_lines):
        return 1

    update_src_cmake(repo_root / "src" / "CMakeLists.txt", module_name)

    print(f"Module '{module_name}' created successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
