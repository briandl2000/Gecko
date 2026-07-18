#!/usr/bin/env bash

set -euo pipefail

Root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
Config="${1:-debug}"
Action="${2:-build}"

case "$Config" in
  debug)
    ConfigName="Debug"
    ConfigFlags=(-O0 -g3 -D_DEBUG=1)
    ;;
  release)
    ConfigName="Release"
    ConfigFlags=(-O2 -g -DNDEBUG=1)
    ;;
  *)
    echo "usage: ./build.sh [debug|release] [build|clean]" >&2
    exit 2
    ;;
esac

PlatformId="Linux-$(uname -m)"
BuildDir="$Root/out/$PlatformId/handmade/$ConfigName"
ObjectDir="$BuildDir/obj"
GeneratedDir="$BuildDir/generated"
BinaryDir="$BuildDir/bin"

if [[ "$Action" == "clean" ]]; then
  rm -rf "$BuildDir"
  exit 0
fi

if [[ "$Action" != "build" ]]; then
  echo "unknown action: $Action" >&2
  exit 2
fi

Cxx="${CXX:-clang++}"
Cc="${CC:-clang}"
Jobs="${GECKO_JOBS:-$(nproc)}"

for Tool in "$Cxx" "$Cc" pkg-config wayland-scanner; do
  if ! command -v "$Tool" >/dev/null 2>&1; then
    echo "required build tool not found: $Tool" >&2
    exit 1
  fi
done

mkdir -p "$ObjectDir" "$GeneratedDir" "$BinaryDir"

ProtocolDir="$(pkg-config --variable=pkgdatadir wayland-protocols)"
XdgShell="$ProtocolDir/stable/xdg-shell/xdg-shell.xml"
XdgDecoration="$ProtocolDir/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"

wayland-scanner client-header "$XdgShell" "$GeneratedDir/xdg-shell-client-protocol.h"
wayland-scanner private-code "$XdgShell" "$GeneratedDir/xdg-shell-protocol.c"
wayland-scanner client-header "$XdgDecoration" "$GeneratedDir/xdg-decoration-client-protocol.h"
wayland-scanner private-code "$XdgDecoration" "$GeneratedDir/xdg-decoration-protocol.c"

CommonFlags=(
  -std=c++23
  -fPIC
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -Wno-unused-parameter
  -Wno-pedantic
  -DGECKO_BUILD_SHARED=1
  -DGECKO_PLATFORM_LINUX=1
  -DGECKO_PLATFORM_LINUX_X11=1
  -DGECKO_PLATFORM_LINUX_WAYLAND=1
  -DGECKO_HAS_XKBCOMMON=1
  -DGECKO_HAVE_XDG_DECORATION=1
  -DGECKO_GRAPHICS_VULKAN=1
  -DGECKO_GRAPHICS_VULKAN_XLIB=1
  -DGECKO_GRAPHICS_VULKAN_WAYLAND=1
  -I"$Root/include"
  -I"$Root/src/core"
  -I"$Root/src/graphics"
  -I"$GeneratedDir"
)

EngineSources=(
  src/core/services.cpp
  src/core/services/engine.cpp
  src/core/services/events.cpp
  src/core/services/jobs.cpp
  src/core/services/log.cpp
  src/core/services/memory.cpp
  src/core/services/modules.cpp
  src/core/services/module_registry.cpp
  src/core/services/profiler.cpp
  src/core/utility/random.cpp
  src/core/utility/thread.cpp
  src/core/utility/time.cpp
  src/core/overwrite_new.cpp

  src/platform/clipboard.cpp
  src/platform/input.cpp
  src/platform/monitors_interface.cpp
  src/platform/platform_config.cpp
  src/platform/platform_io.cpp
  src/platform/platform_module.cpp
  src/platform/terminal.cpp
  src/platform/window_event_input.cpp
  src/platform/windows_interface.cpp
  src/platform/private/null_monitors_backend.cpp
  src/platform/private/null_windows_interface.cpp
  src/platform/linux/platform_io_linux.cpp
  src/platform/linux/threading_linux.cpp
  src/platform/linux/x11_monitors_backend.cpp
  src/platform/linux/x11_windows_interface.cpp
  src/platform/linux/wayland_monitors_backend.cpp
  src/platform/linux/wayland_windows_backend.cpp

  src/runtime/async_trace_profiler_sink.cpp
  src/runtime/console_log_sink.cpp
  src/runtime/crash_safe_trace_profiler_sink.cpp
  src/runtime/event_bus.cpp
  src/runtime/file_log_sink.cpp
  src/runtime/immediate_logger.cpp
  src/runtime/ring_logger.cpp
  src/runtime/ring_profiler.cpp
  src/runtime/runtime_module.cpp
  src/runtime/standard_log_sinks.cpp
  src/runtime/thread_pool_job_system.cpp
  src/runtime/trace_file_sink.cpp
  src/runtime/trace_writer.cpp
  src/runtime/tracking_allocator.cpp

  src/graphics/graphics_device.cpp
  src/graphics/graphics_module.cpp
  src/graphics/private/null_device.cpp
  src/graphics/vulkan/vulkan_command_list.cpp
  src/graphics/vulkan/vulkan_device.cpp
  src/graphics/vulkan/vulkan_gpu_sampler.cpp
  src/graphics/vulkan/vulkan_surface.cpp
  src/graphics/vulkan/linux/vulkan_wayland_surface.cpp
  src/graphics/vulkan/linux/vulkan_xlib_surface.cpp
)

Objects=()
Pending=()

wait_batch()
{
  local Failed=0
  local Pid
  for Pid in "${Pending[@]}"; do
    wait "$Pid" || Failed=1
  done
  Pending=()
  if (( Failed != 0 )); then
    exit 1
  fi
}

echo "Building Gecko $ConfigName ($Jobs parallel jobs)"
for Source in "${EngineSources[@]}"; do
  ObjectName="${Source//\//_}"
  Object="$ObjectDir/${ObjectName%.cpp}.o"
  Objects+=("$Object")
  echo "  CXX $Source"
  "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" -DGECKO_BUILDING=1 \
    -c "$Root/$Source" -o "$Object" &
  Pending+=("$!")
  if (( ${#Pending[@]} >= Jobs )); then
    wait_batch
  fi
done
wait_batch

ProtocolObjects=(
  "$ObjectDir/xdg-shell-protocol.o"
  "$ObjectDir/xdg-decoration-protocol.o"
)
"$Cc" -fPIC -Wall -Wextra -Werror -I"$GeneratedDir" \
  -c "$GeneratedDir/xdg-shell-protocol.c" -o "${ProtocolObjects[0]}"
"$Cc" -fPIC -Wall -Wextra -Werror -I"$GeneratedDir" \
  -c "$GeneratedDir/xdg-decoration-protocol.c" -o "${ProtocolObjects[1]}"

read -r -a PlatformLibraries <<<"$(pkg-config --libs wayland-client wayland-cursor xkbcommon x11 xrandr vulkan)"

echo "  LINK libGecko.so"
"$Cxx" -shared -fuse-ld=lld -Wl,-soname,libGecko.so \
  "${Objects[@]}" "${ProtocolObjects[@]}" \
  "${PlatformLibraries[@]}" -pthread -lm \
  -o "$BinaryDir/libGecko.so"

echo "  CXX app_skeleton"
"$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" \
  "$Root/examples/app_skeleton/src/main.cpp" \
  "$Root/examples/app_skeleton/src/App.cpp" \
  -L"$BinaryDir" -lGecko -Wl,-rpath,'$ORIGIN' \
  -o "$BinaryDir/gecko_sandbox"

echo "Built $BinaryDir/gecko_sandbox"
