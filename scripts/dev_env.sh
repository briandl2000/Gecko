#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

export GECKO_PLATFORM_ID="$(uname -s)-$(uname -m)"
export PATH="$REPO_ROOT/bin:$PATH"
