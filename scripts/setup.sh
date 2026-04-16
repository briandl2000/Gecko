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

# Point git to our hooks directory
git -C "$REPO_ROOT" config core.hooksPath .githooks 2>/dev/null

# Platform identifier for build/output directory separation
export GECKO_PLATFORM_ID="$(uname -s)-$(uname -m)"

# Configure CMake if not already done
if [ ! -d "$REPO_ROOT/out/build/$GECKO_PLATFORM_ID" ]; then
    echo "Configuring CMake for $GECKO_PLATFORM_ID..."
    
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
    
    cmake -S "$REPO_ROOT" -B "$REPO_ROOT/out/build/$GECKO_PLATFORM_ID" \
        -G "Ninja Multi-Config" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DGECKO_BUILD_TESTS=ON
fi

echo "Gecko dev environment ready. Commands: gk build, gk test, gk package"
