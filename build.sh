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
    echo "usage: ./build.sh [debug|release] [build|clean|examples]" >&2
    exit 2
    ;;
esac

PlatformId="Linux-$(uname -m)"
BuildDir="$Root/out/$PlatformId/$ConfigName"
ObjectDir="$BuildDir/obj"
GeneratedDir="$BuildDir/generated"
BinaryDir="$BuildDir/bin"

if [[ "$Action" == "clean" ]]; then
  echo "[CLEAN] $BuildDir"
  rm -rf "$BuildDir"
  exit 0
fi

if [[ "$Action" != "build" && "$Action" != "examples" ]]; then
  echo "unknown action: $Action" >&2
  exit 2
fi

Cxx="${CXX:-g++}"
Cc="${CC:-gcc}"
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
  -std=c++26
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
FastLinkFlags=()
if command -v ld.lld >/dev/null 2>&1; then
  FastLinkFlags=(-fuse-ld=lld)
fi

SignatureFile="$ObjectDir/build-signature"
BuildSignature="$Cxx:$($Cxx -dumpfullversion -dumpversion)|$Cc:$($Cc -dumpfullversion -dumpversion)|${CommonFlags[*]}|${ConfigFlags[*]}|${FastLinkFlags[*]}"
PreviousSignature=""
if [[ -f "$SignatureFile" ]]; then
  IFS= read -r PreviousSignature < "$SignatureFile"
fi
ForceRebuild=false
if [[ "$BuildSignature" != "$PreviousSignature" ]]; then
  ForceRebuild=true
fi

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

echo "[GECKO] $ConfigName $Action ($Jobs jobs)"
for Source in "${EngineSources[@]}"; do
  ObjectName="${Source//\//_}"
  Object="$ObjectDir/${ObjectName%.cpp}.o"
  Objects+=("$Object")
  if [[ "$ForceRebuild" == false && -f "$Object" && "$Root/$Source" -ot "$Object" &&
        "$Root/build.sh" -ot "$Object" ]] &&
     ! find "$Root/include" "$Root/src" -type f \( -name '*.h' -o -name '*.cpp' \) -newer "$Object" -print -quit | grep -q .; then
    continue
  fi
  echo "[CXX  ] $Source"
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
if [[ "$ForceRebuild" == true || ! -f "${ProtocolObjects[0]}" ||
      "$GeneratedDir/xdg-shell-protocol.c" -nt "${ProtocolObjects[0]}" ]]; then
  "$Cc" -fPIC -Wall -Wextra -Werror -I"$GeneratedDir" \
    -c "$GeneratedDir/xdg-shell-protocol.c" -o "${ProtocolObjects[0]}"
fi
if [[ "$ForceRebuild" == true || ! -f "${ProtocolObjects[1]}" ||
      "$GeneratedDir/xdg-decoration-protocol.c" -nt "${ProtocolObjects[1]}" ]]; then
  "$Cc" -fPIC -Wall -Wextra -Werror -I"$GeneratedDir" \
    -c "$GeneratedDir/xdg-decoration-protocol.c" -o "${ProtocolObjects[1]}"
fi

read -r -a PlatformLibraries <<<"$(pkg-config --libs wayland-client wayland-cursor xkbcommon x11 xrandr vulkan)"

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
  echo "[LINK ] libGecko.so"
  "$Cxx" "${CommonLinkFlags[@]}" "${FastLinkFlags[@]}" -shared -Wl,-soname,libGecko.so \
    "${Objects[@]}" "${ProtocolObjects[@]}" \
    "${PlatformLibraries[@]}" -pthread -ldl -lm \
    -o "$EngineLibrary"
fi

if [[ "$Action" == "examples" ]]; then
  ToolDir="$BuildDir/tools"
  EmbedTool="$ToolDir/gecko_embed"
  mkdir -p "$ToolDir"
  if [[ "$ForceRebuild" == true || ! -f "$EmbedTool" || "$Root/tools/embed.cpp" -nt "$EmbedTool" ]]; then
    echo "[TOOL ] gecko_embed"
    "$Cxx" -std=c++26 -O2 "$Root/tools/embed.cpp" -o "$EmbedTool"
  fi

  ShaderDir="$GeneratedDir/graphics_example_shaders"
  "$Root/tools/build_shaders.sh" "$Root/examples/graphics_example/shaders" "$ShaderDir"
  echo "[EMBED] graphics_example/shaders.h"
  "$EmbedTool" "$ShaderDir/shaders.h" gecko::examples::graphics_example::shaders \
    "$ShaderDir/triangle.vert.spv" TriangleVert \
    "$ShaderDir/triangle.frag.spv" TriangleFrag \
    "$ShaderDir/fullscreen.vert.spv" FullscreenVert \
    "$ShaderDir/fullscreen.frag.spv" FullscreenFrag \
    "$ShaderDir/plasma.comp.spv" PlasmaComp
  for Example in app_skeleton core_example math_example platform_example graphics_example; do
    Executable="$BinaryDir/gecko_example_$Example"
    ExampleSources=("$Root/examples/$Example/src/"*.cpp)
    Rebuild=false
    if [[ ! -f "$Executable" || "$EngineLibrary" -nt "$Executable" || "$Root/build.sh" -nt "$Executable" ]]; then
      Rebuild=true
    else
      for Source in "${ExampleSources[@]}"; do
        if [[ "$Source" -nt "$Executable" ]]; then
          Rebuild=true
          break
        fi
      done
    fi
    if [[ "$Rebuild" == true ]] ||
       find "$Root/include" -type f -name '*.h' -newer "$Executable" -print -quit | grep -q .; then
      echo "[LINK ] gecko_example_$Example"
      "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" "${CommonLinkFlags[@]}" \
        -I"$ShaderDir" "${ExampleSources[@]}" -L"$BinaryDir" -lGecko -lm -Wl,-rpath,'$ORIGIN' -o "$Executable"
    fi
  done
  printf '%s\n' "$BuildSignature" > "$SignatureFile"
  echo "[DONE ] $BinaryDir/gecko_example_*"
  exit 0
fi

GameLibrary="$BinaryDir/libgecko_game.so"
if [[ ! -f "$GameLibrary" || "$Root/projects/sandbox/game.cpp" -nt "$GameLibrary" || "$EngineLibrary" -nt "$GameLibrary" ]] ||
   find "$Root/include" -type f -name '*.h' -newer "$GameLibrary" -print -quit | grep -q .; then
  echo "[LINK ] libgecko_game.so"
  "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" "${CommonLinkFlags[@]}" -shared \
    "$Root/projects/sandbox/game.cpp" \
    -L"$BinaryDir" -lGecko -Wl,-rpath,'$ORIGIN' \
    -o "$GameLibrary"
fi

Launcher="$BinaryDir/gecko_launcher"
if [[ ! -f "$Launcher" || "$Root/projects/launcher/main.cpp" -nt "$Launcher" || "$EngineLibrary" -nt "$Launcher" ]] ||
   find "$Root/include" -type f -name '*.h' -newer "$Launcher" -print -quit | grep -q .; then
  echo "[LINK ] gecko_launcher"
  "$Cxx" "${CommonFlags[@]}" "${ConfigFlags[@]}" "${CommonLinkFlags[@]}" \
    "$Root/projects/launcher/main.cpp" \
    -L"$BinaryDir" -lGecko -Wl,-rpath,'$ORIGIN' \
    -o "$Launcher"
fi

printf '%s\n' "$BuildSignature" > "$SignatureFile"
echo "[DONE ] $BinaryDir/gecko_launcher"
