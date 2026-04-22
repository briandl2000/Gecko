#!/usr/bin/env bash
# Fetch a prebuilt aarch64 libvulkan.so.1 from the Pi for cross-compile linking.
# Headers are pulled via FetchContent (arch-neutral), but the linker needs a
# real .so that matches the target SONAME.
#
# Usage (once, after connecting a Pi):
#   scripts/fetch_pi_vulkan.sh
# Env:
#   GECKO_PI_HOST  (default: pi@raspberrypi.local)

set -euo pipefail

PI_HOST="${GECKO_PI_HOST:-pi@raspberrypi.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO_ROOT/third_party/prebuilt/Linux-aarch64"
HEADERS_DIR="$REPO_ROOT/third_party/vulkan-headers"
HEADERS_TAG="vulkan-sdk-1.3.290.0"

mkdir -p "$DEST"

# 1. Clone Vulkan-Headers once (arch-neutral) — used by any cross build.
if [[ ! -f "$HEADERS_DIR/include/vulkan/vulkan.h" ]]; then
  echo "Cloning Vulkan-Headers ($HEADERS_TAG) ..."
  git clone --depth 1 --branch "$HEADERS_TAG" \
    https://github.com/KhronosGroup/Vulkan-Headers "$HEADERS_DIR"
else
  echo "Vulkan-Headers already present: $HEADERS_DIR"
fi

echo "Fetching aarch64 link libraries from $PI_HOST ..."
# Core Vulkan loader + X11/XCB/Wayland client libs used by Gecko's graphics
# surface backends.  Everything lives under /usr/lib/aarch64-linux-gnu/ on
# Debian-based Pi OS.
libs=(
  "libvulkan.so.1"
  "libX11.so.6"
  "libxcb.so.1"
  "libwayland-client.so.0"
)

for lib in "${libs[@]}"; do
  if scp -q "$PI_HOST:/usr/lib/aarch64-linux-gnu/$lib" "$DEST/$lib" 2>/dev/null; then
    echo "  ✓ $lib"
    # Also create the un-versioned symlink the linker looks for
    base="${lib%%.so*}"
    (cd "$DEST" && ln -sf "$lib" "${base}.so")
  else
    echo "  ✗ $lib (not available on target; skipping)"
  fi
done

# Fetch the matching arch-neutral dev headers from the Pi.  We can't use the
# host's /usr/include because -I/usr/include poisons system includes (pulls
# x86_64 stdint/stddef into an aarch64 compile, breaking uintptr_t etc).
HEADERS_PKG_DIR="$REPO_ROOT/third_party/prebuilt/include"
mkdir -p "$HEADERS_PKG_DIR"
echo ""
echo "Fetching surface/display headers from $PI_HOST ..."
# rsync is more efficient than scp -r and preserves directory layout
header_roots=(
  "/usr/include/X11"
  "/usr/include/wayland-client.h"
  "/usr/include/wayland-client-core.h"
  "/usr/include/wayland-client-protocol.h"
  "/usr/include/wayland-util.h"
  "/usr/include/wayland-version.h"
  "/usr/include/xcb"
)
for h in "${header_roots[@]}"; do
  if rsync -az --quiet "$PI_HOST:$h" "$HEADERS_PKG_DIR/" 2>/dev/null; then
    echo "  ✓ $(basename "$h")"
  else
    echo "  ✗ $(basename "$h") (skipping)"
  fi
done

echo ""
echo "Done. Files in: $DEST"
ls -la "$DEST"
