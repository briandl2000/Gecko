# syntax=docker/dockerfile:1.6
# Gecko aarch64 build image.
# Runs Ubuntu 24.04 arm64 under QEMU binfmt_misc on x86_64 dev hosts so CMake
# configures exactly like it would on a real Pi.
#
# Build once:
#   docker buildx build --platform=linux/arm64 \
#       -t gecko-aarch64-dev -f docker/aarch64.Dockerfile .
#
# Everyday use: `source scripts/pi-dev.sh` then `gk-pi <subcommand>` — the
# helper script builds this image automatically on first run.
FROM --platform=linux/arm64 ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8

RUN apt-get update \
 && apt-get install -y --no-install-recommends software-properties-common ca-certificates \
 && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
        gcc-15 g++-15 \
        cmake ninja-build pkg-config \
        python3 python3-pip \
        git curl rsync \
        libvulkan-dev vulkan-validationlayers spirv-tools glslc \
        libx11-dev libxext-dev libxrandr-dev \
        libwayland-dev wayland-protocols libxkbcommon-dev \
 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100 \
 && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-15 \
    CXX=g++-15 \
    GECKO_PLATFORM_ID=Linux-aarch64

WORKDIR /workspace
# syntax=docker/dockerfile:1.6
# Gecko aarch64 build image (Debian trixie: ships GCC 15 natively, no PPA needed).
# Runs under QEMU binfmt_misc on x86_64 dev hosts.
#
# Build:   docker buildx build --platform=linux/arm64 -t gecko-aarch64-dev -f docker/aarch64.Dockerfile .
# Use via: scripts/pi-dev.sh (sourced in shell) → gk-pi <subcommand>
FROM --platform=linux/arm64 debian:trixie-slim

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        gcc-15 g++-15 \
        cmake ninja-build pkg-config \
        python3 python3-pip \
        git ca-certificates curl rsync \
        libvulkan-dev vulkan-validationlayers spirv-tools glslc \
        libx11-dev libxext-dev libxrandr-dev \
        libwayland-dev wayland-protocols libxkbcommon-dev \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100 \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-15 \
    CXX=g++-15 \
    GECKO_PLATFORM_ID=Linux-aarch64

WORKDIR /workspace
