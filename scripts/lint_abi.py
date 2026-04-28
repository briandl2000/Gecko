#!/usr/bin/env python3
"""Lint that no `std::` types appear in CoreServices virtual signatures.

The `CoreServices` shared library is the only thing in Gecko whose ABI must
remain compiler-stable. Every virtual method on its interfaces (`IAllocator`,
`ILogger`, `IProfiler`, `IModule`, `IEventBus`, `IProfilerSink`, ...) must use
only:

- primitives,
- raw pointers,
- `const char*`,
- Gecko PODs,
- `gecko::Span<T>` (see include/gecko/core/span.h).

`std::string`, `std::string_view`, `std::span`, `std::vector`, `std::function`,
... are forbidden because their layout / vtable / allocator behavior differs
between MSVC, libstdc++, and libc++.

This is a coarse line-level grep. It deliberately scans only the public
interface headers; static-library internals can use std:: freely. False
positives can be silenced with a trailing `// abi-ok: <reason>` comment on
the offending line.

Exit codes:
    0  no violations
    1  one or more violations
    2  usage error
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Public headers whose declarations cross the CoreServices shared-library
# boundary. Scanned line-by-line for `std::` types.
#
# Includes:
#   - include/gecko/core/services/  (the IService interfaces themselves)
#   - include/gecko/core/engine.h   (Engine::Create / dtor / move are
#                                    GECKO_API and the implementation lives
#                                    in CoreServices.dll)
#
# Other headers under include/gecko/{platform,graphics,runtime}/ live in
# STATIC libraries -- their GECKO_API decoration is a no-op and they share
# a TU/STL with their consumer, so std:: types are safe there.
_SCANNED_DIRS = [
    "include/gecko/core/services",
]
_SCANNED_FILES = [
    "include/gecko/core/engine.h",
]

# Pattern matching `std::<identifier>` anywhere on a line. We rely on the
# scanned set being small enough that hand-allowlisting via inline comments
# is cheap.
_STD_PATTERN = re.compile(r"\bstd\s*::\s*[A-Za-z_][A-Za-z0-9_]*")

# Marker that suppresses violations. Use sparingly with a justification.
# Place it either on the offending line itself (trailing comment) or on
# any preceding line of the same logical statement (separated by no blank
# line), so it survives clang-format wrapping.
#
# For longer header-only template/inline regions where every line would
# otherwise need a marker, wrap the block with:
#   // abi-ok-begin: <reason>
#   ... code ...
#   // abi-ok-end
_OK_MARKER = "// abi-ok"
_REGION_BEGIN = "// abi-ok-begin"
_REGION_END = "// abi-ok-end"


def _scan_file(path: Path) -> list[tuple[int, str]]:
    violations: list[tuple[int, str]] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    in_region = False
    for idx, line in enumerate(lines):
        lineno = idx + 1
        if _REGION_BEGIN in line:
            in_region = True
            continue
        if _REGION_END in line:
            in_region = False
            continue
        if in_region:
            continue
        # Inline marker on the offending line itself.
        if _OK_MARKER in line:
            continue
        # Block marker: an `// abi-ok` comment line scoped to the
        # immediately following statement (until the next blank line).
        # Walk backwards through contiguous non-blank lines and accept if any
        # of them contains the marker.
        suppressed = False
        for back in range(idx - 1, -1, -1):
            prev = lines[back]
            if not prev.strip():
                break
            if _OK_MARKER in prev:
                suppressed = True
                break
        if suppressed:
            continue
        # Skip pure comment lines.
        stripped = line.lstrip()
        if stripped.startswith("//") or stripped.startswith("*") or stripped.startswith("///"):
            continue
        # Drop everything after the first `//` so trailing `// std::foo` in a
        # comment doesn't trigger.
        code = line.split("//", 1)[0]
        if _STD_PATTERN.search(code):
            violations.append((lineno, line.rstrip()))
    return violations


def main(argv: list[str]) -> int:
    repo_root = Path(__file__).resolve().parent.parent
    if len(argv) > 1:
        repo_root = Path(argv[1]).resolve()

    bad: dict[Path, list[tuple[int, str]]] = {}
    for rel in _SCANNED_DIRS:
        scan_dir = repo_root / rel
        if not scan_dir.is_dir():
            print(f"lint_abi: missing scan directory: {scan_dir}", file=sys.stderr)
            return 2
        for header in sorted(scan_dir.rglob("*.h")):
            violations = _scan_file(header)
            if violations:
                bad[header.relative_to(repo_root)] = violations
    for rel in _SCANNED_FILES:
        scan_file = repo_root / rel
        if not scan_file.is_file():
            print(f"lint_abi: missing scan file: {scan_file}", file=sys.stderr)
            return 2
        violations = _scan_file(scan_file)
        if violations:
            bad[scan_file.relative_to(repo_root)] = violations

    if not bad:
        print("lint_abi: clean")
        return 0

    print("lint_abi: forbidden std:: types in CoreServices interface headers:")
    for path, hits in bad.items():
        for lineno, text in hits:
            print(f"  {path}:{lineno}: {text}")
    print()
    print("Use a Gecko POD or gecko::Span<T> instead, or add a trailing")
    print(f"`{_OK_MARKER}: <reason>` comment if the use is genuinely safe.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
