#!/bin/bash
# Format all C++ source files using clang-format

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "Formatting all C++ files in the project..."

find src include examples -type f \( -name "*.cpp" -o -name "*.h" \) -print0 | \
  xargs -0 clang-format -i

echo "Done! All files formatted."
