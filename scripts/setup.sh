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
    
    # Require Clang compiler
    if command -v clang &> /dev/null && command -v clang++ &> /dev/null; then
        echo "Using Clang compiler: $(clang --version | head -n1)"
        export CC=clang
        export CXX=clang++
    else
        echo "ERROR: Clang compiler not found!"
        echo "Install with:"
        echo "  Ubuntu/Debian: sudo apt-get install clang"
        echo "  Fedora:        sudo dnf install clang"
        echo "  macOS:         brew install llvm"
        return 1
    fi
    
    cmake -S "$REPO_ROOT" -B "$REPO_ROOT/out/build" -G "Ninja Multi-Config"
fi

echo "Gecko dev environment ready. Commands: gk build, gk test, gk package"
