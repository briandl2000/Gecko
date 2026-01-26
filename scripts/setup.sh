#!/usr/bin/env bash
# Gecko Development Environment
# Source this file: source scripts/setup.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Create gk function (doesn't pollute PATH)
gk() {
    python3 "$REPO_ROOT/scripts/cli.py" "$@"
}
export -f gk

# Configure CMake if not already done
if [ ! -d "$REPO_ROOT/out/build" ]; then
    echo "Configuring CMake..."
    cmake --preset debug -S "$REPO_ROOT"
fi

echo "Gecko dev environment ready. Commands: gk build, gk test, gk package"
