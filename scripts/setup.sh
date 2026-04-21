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
_gecko_os="$(uname -s)"
case "$_gecko_os" in
    MINGW*|MSYS*) _gecko_os="Windows" ;;
esac
export GECKO_PLATFORM_ID="${_gecko_os}-$(uname -m)"
unset _gecko_os

# Configure CMake if not already done
if [ ! -d "$REPO_ROOT/out/build/$GECKO_PLATFORM_ID" ]; then
    echo "Configuring CMake for $GECKO_PLATFORM_ID..."
    
    # Prefer GCC; fall back to any cc/c++ if available
    if command -v gcc &> /dev/null && command -v g++ &> /dev/null; then
        echo "Using GCC compiler: $(gcc --version | head -n1)"
        export CC=gcc
        export CXX=g++
    else
        echo "ERROR: GCC compiler not found!"
        echo "Install with:"
        echo "  Ubuntu/Debian: sudo apt-get install gcc g++"
        echo "  Fedora:        sudo dnf install gcc gcc-c++"
        echo "  Arch:          sudo pacman -S gcc"
        return 1
    fi
    
    cmake -S "$REPO_ROOT" -B "$REPO_ROOT/out/build/$GECKO_PLATFORM_ID" \
        -G "Ninja Multi-Config" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DGECKO_BUILD_TESTS=ON
fi

echo "Gecko dev environment ready. Commands: gk build, gk test, gk package"
