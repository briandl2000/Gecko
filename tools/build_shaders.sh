#!/usr/bin/env bash

set -euo pipefail

if (( $# != 2 )); then
  echo "usage: tools/build_shaders.sh <source-dir> <output-dir>" >&2
  exit 2
fi

SourceDir="$(cd "$1" && pwd)"
OutputDir="$2"
Compiler="${GLSLC:-}"

if [[ -z "$Compiler" ]] && command -v glslc >/dev/null 2>&1; then
  Compiler="$(command -v glslc)"
fi
if [[ -z "$Compiler" ]] && [[ -n "${VULKAN_SDK:-}" ]] && [[ -x "$VULKAN_SDK/bin/glslc" ]]; then
  Compiler="$VULKAN_SDK/bin/glslc"
fi
if [[ -z "$Compiler" ]]; then
  echo "glslc was not found; install the Vulkan SDK or set GLSLC" >&2
  exit 1
fi

mkdir -p "$OutputDir"
Found=false
while IFS= read -r -d '' Source; do
  Found=true
  File="${Source##*/}"
  Stage=""
  case "$File" in
    *.vert.hlsl) Stage="vert" ;;
    *.frag.hlsl) Stage="frag" ;;
    *.comp.hlsl) Stage="comp" ;;
    *.geom.hlsl) Stage="geom" ;;
    *.tesc.hlsl) Stage="tesc" ;;
    *.tese.hlsl) Stage="tese" ;;
    *) continue ;;
  esac

  Output="$OutputDir/${File%.hlsl}.spv"
  if [[ -f "$Output" && "$Source" -ot "$Output" ]] &&
     ! find "$SourceDir" -type f \( -name '*.hlsl' -o -name '*.hlsli' \) -newer "$Output" -print -quit | grep -q .; then
    continue
  fi
  echo "  GLSLC ${Source#$SourceDir/}"
  "$Compiler" -x hlsl -fshader-stage="$Stage" -fentry-point=main -I"$SourceDir" "$Source" -o "$Output"
done < <(find "$SourceDir" -type f -name '*.hlsl' -print0 | sort -z)

if [[ "$Found" == false ]]; then
  echo "no .hlsl shaders found in $SourceDir" >&2
  exit 1
fi
