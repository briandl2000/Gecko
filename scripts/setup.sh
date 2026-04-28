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

    # Let CMake auto-detect the compiler. Honour CC/CXX if the caller
    # already exported them; otherwise CMake picks whatever is available
    # (gcc on Linux, MSVC on Windows, the cross toolchain when invoked
    # via gk-pi).
    if [ -z "${CXX:-}" ] && ! command -v c++ &> /dev/null \
        && ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
        echo "ERROR: No C++ compiler found on PATH."
        echo "Install one of:"
        echo "  Ubuntu/Debian: sudo apt-get install build-essential"
        echo "  Fedora:        sudo dnf install gcc gcc-c++"
        echo "  Arch:          sudo pacman -S base-devel"
        return 1
    fi

    cmake -S "$REPO_ROOT" -B "$REPO_ROOT/out/build/$GECKO_PLATFORM_ID" \
        -G "Ninja Multi-Config" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DGECKO_BUILD_TESTS=ON
fi

echo "Gecko dev environment ready. Commands: gk build, gk test, gk package"
