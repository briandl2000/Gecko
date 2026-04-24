#!/usr/bin/env bash
# Gecko: Raspberry Pi / aarch64 development helpers.
#
# Source this file in your shell to get the `gk-pi` function:
#
#     source scripts/pi-dev.sh
#     gk-pi build debug              # builds inside aarch64 container
#     gk-pi test  debug              # builds + runs tests (headless, in container)
#     gk-pi deploy debug             # rsync built binaries to the Pi
#     gk-pi run graphics_example     # deploy + ssh run on the Pi
#     gk-pi gdbserver graphics_example debug   # deploy + remote gdbserver
#     gk-pi fetch-logs               # pull ~/gecko/working_dir/ back
#
# Builds run in an x86_64 container that carries an aarch64-linux-gnu cross
# toolchain (no QEMU — host-speed compile). Deploy / run / gdbserver use
# plain ssh+rsync to the Pi.
#
# Host / paths / SSH target come from ~/.gecko-pi.env (see .gecko-pi.env.example).

set -u

GECKO_PI_DEV_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GECKO_REPO_ROOT="$(cd "$GECKO_PI_DEV_SCRIPT_DIR/.." && pwd)"
GECKO_PI_IMAGE_TAG="gecko-aarch64-dev"
GECKO_PI_DOCKERFILE="$GECKO_REPO_ROOT/docker/aarch64.Dockerfile"

# ---- config ----------------------------------------------------------------
# Load user config if present; otherwise apply sensible defaults. Missing values
# only matter for SSH subcommands (deploy/run/gdbserver/fetch-logs).
if [[ -f "$GECKO_REPO_ROOT/.gecko-pi.env" ]]; then
    # shellcheck disable=SC1091
    source "$GECKO_REPO_ROOT/.gecko-pi.env"
fi
: "${GECKO_PI_HOST:=pi@raspberrypi.local}"
: "${GECKO_PI_DIR:=~/gecko}"
: "${GECKO_PI_GDB_PORT:=2345}"
export GECKO_PI_HOST GECKO_PI_DIR GECKO_PI_GDB_PORT

_gkpi_log() { printf '[gk-pi] %s\n' "$*"; }
_gkpi_err() { printf '[gk-pi] error: %s\n' "$*" >&2; }

# ---- docker plumbing -------------------------------------------------------
_gkpi_require_docker() {
    if ! command -v docker >/dev/null 2>&1; then
        _gkpi_err "docker not found in PATH. Install:"
        _gkpi_err "    sudo pacman -S docker                           # Arch"
        _gkpi_err "    sudo apt-get install docker.io                  # Debian/Ubuntu"
        return 1
    fi
}

_gkpi_ensure_image() {
    _gkpi_require_docker || return 1
    if docker image inspect "$GECKO_PI_IMAGE_TAG" >/dev/null 2>&1; then
        return 0
    fi
    _gkpi_log "aarch64 image '$GECKO_PI_IMAGE_TAG' not present — building (one-time, ~2 min)..."
    docker build \
        -t "$GECKO_PI_IMAGE_TAG" \
        -f "$GECKO_PI_DOCKERFILE" \
        "$GECKO_REPO_ROOT" || return 1
}

_gkpi_docker_run() {
    _gkpi_ensure_image || return 1
    docker run --rm -t \
        --user "$(id -u):$(id -g)" \
        -v "$GECKO_REPO_ROOT":/workspace \
        -w /workspace \
        -e HOME=/tmp \
        -e GECKO_PLATFORM_ID=Linux-aarch64 \
        -e CMAKE_TOOLCHAIN_FILE=/workspace/docker/aarch64-toolchain.cmake \
        "$GECKO_PI_IMAGE_TAG" \
        "$@"
}

# ---- subcommands -----------------------------------------------------------
_gkpi_cfg_cased() {
    case "${1:-debug}" in
        debug|Debug|DEBUG)     echo "Debug" ;;
        release|Release|RELEASE) echo "Release" ;;
        *) echo "$1" ;;
    esac
}

_gkpi_deploy() {
    local cfg
    cfg="$(_gkpi_cfg_cased "${1:-debug}")"
    local bin_dir="$GECKO_REPO_ROOT/out/Linux-aarch64/bin/$cfg"
    if [[ ! -d "$bin_dir" ]]; then
        _gkpi_err "no binaries at $bin_dir — run 'gk-pi build ${1:-debug}' first"
        return 1
    fi
    _gkpi_log "deploying $cfg → $GECKO_PI_HOST:$GECKO_PI_DIR/"
    ssh "$GECKO_PI_HOST" "mkdir -p $GECKO_PI_DIR/working_dir $GECKO_PI_DIR/tests"
    rsync -az "$bin_dir/" "$GECKO_PI_HOST:$GECKO_PI_DIR/"
    [[ -d "$GECKO_REPO_ROOT/working_dir" ]] && \
        rsync -az "$GECKO_REPO_ROOT/working_dir/" "$GECKO_PI_HOST:$GECKO_PI_DIR/working_dir/" || true
}

_gkpi_run() {
    local target="${1:?usage: gk-pi run <target> [config]}"
    local cfg="${2:-debug}"
    _gkpi_deploy "$cfg" || return $?
    _gkpi_log "ssh → $GECKO_PI_HOST: $target"
    ssh -t "$GECKO_PI_HOST" \
        "cd $GECKO_PI_DIR/working_dir && LD_LIBRARY_PATH=$GECKO_PI_DIR $GECKO_PI_DIR/$target"
}

_gkpi_gdbserver() {
    local target="${1:?usage: gk-pi gdbserver <target> [config]}"
    local cfg="${2:-debug}"
    _gkpi_deploy "$cfg" || return $?
    _gkpi_log "starting gdbserver on $GECKO_PI_HOST:$GECKO_PI_GDB_PORT for $target"
    ssh "$GECKO_PI_HOST" \
        "pkill gdbserver 2>/dev/null; sleep 0.3; \
         cd $GECKO_PI_DIR/working_dir && \
         LD_LIBRARY_PATH=$GECKO_PI_DIR gdbserver :$GECKO_PI_GDB_PORT $GECKO_PI_DIR/$target"
}

_gkpi_fetch_logs() {
    _gkpi_log "rsync $GECKO_PI_HOST:$GECKO_PI_DIR/working_dir/ → working_dir/"
    rsync -az "$GECKO_PI_HOST:$GECKO_PI_DIR/working_dir/" "$GECKO_REPO_ROOT/working_dir/"
}

# ---- entry point -----------------------------------------------------------
gk-pi() {
    local sub="${1:-help}"
    case "$sub" in
        deploy)      shift; _gkpi_deploy "$@" ;;
        run)         shift; _gkpi_run "$@" ;;
        gdbserver)   shift; _gkpi_gdbserver "$@" ;;
        fetch-logs)  shift; _gkpi_fetch_logs ;;
        shell)       shift; _gkpi_docker_run bash ;;
        rebuild-image)
            _gkpi_require_docker || return 1
            docker rmi "$GECKO_PI_IMAGE_TAG" 2>/dev/null || true
            _gkpi_ensure_image
            ;;
        help|-h|--help)
            cat <<EOF
gk-pi — aarch64 / Raspberry Pi workflow

  build | test | format | clean | <any gk verb>
      Forwarded to gk inside the aarch64 container.
      Artefacts appear in out/Linux-aarch64/bin/<Config>/.

  deploy [config]
      rsync out/Linux-aarch64/bin/<Config>/ to \$GECKO_PI_HOST:\$GECKO_PI_DIR/.

  run <target> [config]
      deploy then ssh-run on the Pi.

  gdbserver <target> [config]
      deploy then start remote gdbserver on :\$GECKO_PI_GDB_PORT.

  fetch-logs
      rsync \$GECKO_PI_DIR/working_dir/ back to repo-local working_dir/.

  shell
      Interactive bash inside the aarch64 container.

  rebuild-image
      Discard the cached image and build it fresh.

Configure host/paths via ~/.gecko-pi.env or <repo>/.gecko-pi.env
(see .gecko-pi.env.example). Defaults: pi@raspberrypi.local, ~/gecko, :2345.
EOF
            ;;
        *)
            # Forward every other verb (build/test/format/clean/run-host/...) to gk
            # inside the container.
            _gkpi_docker_run python3 scripts/cli.py "$@"
            ;;
    esac
}

_gkpi_log "loaded. Host: $GECKO_PI_HOST  Path: $GECKO_PI_DIR. Try 'gk-pi help'."
