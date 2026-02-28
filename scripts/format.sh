#!/bin/bash
# Format all C++ source files using the Python formatter for portability.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

python "$PROJECT_ROOT/scripts/format.py"
