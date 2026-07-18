#!/usr/bin/env bash

set -euo pipefail

Root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
Config="${1:-debug}"
Action="${2:-build}"
GameProject="${GECKO_GAME:-sandbox}"

if [[ ! "$GameProject" =~ ^[A-Za-z0-9_-]+$ ]]; then
  echo "GECKO_GAME must be a project name from projects/" >&2
  exit 2
fi

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
    echo "usage: ./build.sh [debug|release] [build|clean|monolithic|examples]" >&2
    exit 2
    ;;
esac

PlatformId="Linux-$(uname -m)"
BuildDir="$Root/out/$PlatformId/handmade/$ConfigName"
ObjectDir="$BuildDir/obj"
GeneratedDir="$BuildDir/generated"
BinaryDir="$BuildDir/bin"
GameSource="$Root/projects/$GameProject/game.cpp"

if [[ "$Action" == "clean" ]]; then
  rm -rf "$BuildDir"
  exit 0
fi

if [[ "$Action" != "build" && "$Action" != "monolithic" && "$Action" != "examples" ]]; then
  echo "unknown action: $Action" >&2
  exit 2
fi

if [[ "$Action" != "examples" && ! -f "$GameSource" ]]; then
  echo "game project not found: projects/$GameProject/game.cpp" >&2
  exit 1
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

if [[ ! -f "$GeneratedDir/xdg-shell-client-protocol.h" || "$XdgShell" -nt "$GeneratedDir/xdg-shell-client-protocol.h" ]]; then
  wayland-scanner client-header "$XdgShell" "$GeneratedDir/xdg-shell-client-protocol.h"
  wayland-scanner private-code "$XdgShell" "$GeneratedDir/xdg-shell-protocol.c"
fi
if [[ ! -f "$GeneratedDir/xdg-decoration-client-protocol.h" || "$XdgDecoration" -nt "$GeneratedDir/xdg-decoration-client-protocol.h" ]]; then
  wayland-scanner client-header "$XdgDecoration" "$GeneratedDir/xdg-decoration-client-protocol.h"
  wayland-scanner private-code "$XdgDecoration" "$GeneratedDir/xdg-decoration-protocol.c"
fi

CommonFlags=(
  -std=c++23
  -fPIC
  -fno-exceptions
  -fno-rtti
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

CommonLinkFlags=(
  -nostdlib++
)

EngineSources=(
  src/gecko_engine.cpp
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
  if [[ -f "$Object" && "$Root/$Source" -ot "$Object" ]] &&
     ! find "$Root/include" "$Root/src" -type f \( -name '*.h' -o -name '*.cpp' \) -newer "$Object" -print -quit | grep -q .; then
    continue
  fi
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
if [[ ! -f "${ProtocolObjects[0]}" || "$GeneratedDir/xdg-shell-protocol.c" -nt "${ProtocolObjects[0]}" ]]; then
  "$Cc" -fPIC -Wall -Wextra -Werror -I"$GeneratedDir" \
    -c "$GeneratedDir/xdg-shell-protocol.c" -o "${ProtocolObjects[0]}"
fi
if [[ ! -f "${ProtocolObjects[1]}" || "$GeneratedDir/xdg-decoration-protocol.c" -nt "${ProtocolObjects[1]}" ]]; then
  "$Cc" -fPIC -Wall -Wextra -Werror -I"$GeneratedDir" \
    -c "$GeneratedDir/xdg-decoration-protocol.c" -o "${ProtocolObjects[1]}"
fi

read -r -a PlatformLibraries <<<"$(pkg-config --libs wayland-client wayland-cursor xkbcommon x11 xrandr vulkan)"

if [[ "$Action" == "monolithic" ]]; then
  Monolithic="$BinaryDir/gecko_monolithic"
  echo "  LINK gecko_monolithic"
  "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" "${CommonLinkFlags[@]}" \
    -DGECKO_MONOLITHIC_GAME=1 \
    "$Root/projects/launcher/main.cpp" "$GameSource" \
    "${Objects[@]}" "${ProtocolObjects[@]}" \
    "${PlatformLibraries[@]}" -pthread -ldl -lm \
    -o "$Monolithic"
  echo "Built $Monolithic"
  exit 0
fi

EngineLibrary="$BinaryDir/libGecko.so"
LinkEngine=false
if [[ ! -f "$EngineLibrary" ]]; then
  LinkEngine=true
else
  for Object in "${Objects[@]}" "${ProtocolObjects[@]}"; do
    if [[ "$Object" -nt "$EngineLibrary" ]]; then
      LinkEngine=true
      break
    fi
  done
fi
if [[ "$LinkEngine" == true ]]; then
  echo "  LINK libGecko.so"
  "$Cxx" "${CommonLinkFlags[@]}" -shared -fuse-ld=lld -Wl,-soname,libGecko.so \
    "${Objects[@]}" "${ProtocolObjects[@]}" \
    "${PlatformLibraries[@]}" -pthread -ldl -lm \
    -o "$EngineLibrary"
fi

if [[ "$Action" == "examples" ]]; then
  "$Root/tools/build_shaders.sh" "$Root/examples/triangle/shaders" "$BinaryDir/shaders"
  for Example in core window triangle; do
    Executable="$BinaryDir/gecko_example_$Example"
    Source="$Root/examples/$Example/main.cpp"
    if [[ ! -f "$Executable" || "$Source" -nt "$Executable" || "$EngineLibrary" -nt "$Executable" ]] ||
       find "$Root/include" -type f -name '*.h' -newer "$Executable" -print -quit | grep -q .; then
      echo "  LINK gecko_example_$Example"
      "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" "${CommonLinkFlags[@]}" \
        "$Source" -L"$BinaryDir" -lGecko -Wl,-rpath,'$ORIGIN' -o "$Executable"
    fi
  done
  echo "Built Gecko examples in $BinaryDir"
  exit 0
fi

GameShaderDir="$Root/projects/$GameProject/shaders"
if [[ -d "$GameShaderDir" ]] && find "$GameShaderDir" -type f -name '*.hlsl' -print -quit | grep -q .; then
  "$Root/tools/build_shaders.sh" "$GameShaderDir" "$BinaryDir/shaders"
fi

GameLibrary="$BinaryDir/libgecko_game.so"
GameStamp="$ObjectDir/game-project"
BuiltGame=""
if [[ -f "$GameStamp" ]]; then
  IFS= read -r BuiltGame < "$GameStamp"
fi
if [[ "$BuiltGame" != "$GameProject" || ! -f "$GameLibrary" || "$GameSource" -nt "$GameLibrary" || "$EngineLibrary" -nt "$GameLibrary" ]] ||
   find "$Root/include" -type f -name '*.h' -newer "$GameLibrary" -print -quit | grep -q .; then
  echo "  LINK libgecko_game.so ($GameProject)"
  "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" "${CommonLinkFlags[@]}" -shared \
    "$GameSource" \
    -L"$BinaryDir" -lGecko -Wl,-rpath,'$ORIGIN' \
    -o "$GameLibrary"
  printf '%s\n' "$GameProject" > "$GameStamp"
fi

Launcher="$BinaryDir/gecko_launcher"
if [[ ! -f "$Launcher" || "$Root/projects/launcher/main.cpp" -nt "$Launcher" || "$EngineLibrary" -nt "$Launcher" ]] ||
   find "$Root/include" -type f -name '*.h' -newer "$Launcher" -print -quit | grep -q .; then
  echo "  LINK gecko_launcher"
  "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" "${CommonLinkFlags[@]}" \
    "$Root/projects/launcher/main.cpp" \
    -L"$BinaryDir" -lGecko -Wl,-rpath,'$ORIGIN' \
    -o "$Launcher"
fi

echo "Built $BinaryDir/gecko_launcher and $BinaryDir/libgecko_game.so ($GameProject)"
