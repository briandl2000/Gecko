#!/usr/bin/env bash

set -euo pipefail

Root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Name="${1:-}"

if [[ -z "$Name" || ! "$Name" =~ ^[A-Za-z][A-Za-z0-9_-]*$ ]]; then
  echo "usage: tools/new_project.sh <name>" >&2
  echo "name must start with a letter and contain only letters, numbers, _ or -" >&2
  exit 2
fi

Destination="$Root/projects/$Name"
if [[ -e "$Destination" ]]; then
  echo "project already exists: $Destination" >&2
  exit 1
fi

mkdir -p "$Destination/shaders"
cp "$Root/projects/sandbox/game.cpp" "$Destination/game.cpp"
sed -i \
  -e "s/game\.sandbox/game.$Name/g" \
  -e "s/Gecko Sandbox/$Name/g" \
  -e "s/Sandbox/$Name/g" \
  "$Destination/game.cpp"

echo "Created projects/$Name"
echo "Build: GECKO_GAME=$Name ./build.sh debug"
