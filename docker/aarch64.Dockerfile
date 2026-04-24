# syntax=docker/dockerfile:1.6
# Gecko aarch64 cross-compile image.
#
# x86_64 Ubuntu 25.04 + aarch64-linux-gnu GCC 15 + arm64 multi-arch runtime
# libs (Vulkan, X11, Wayland). No QEMU — the compiler runs natively on the
# host and emits aarch64 code at host speed.
#
# Ubuntu 25.04 is used because it ships gcc-15 natively (including the
# aarch64-linux-gnu cross-compiler) in the standard repositories.
#
# Build once:
#   docker build -t gecko-aarch64-dev -f docker/aarch64.Dockerfile .
#
# Everyday use: `source scripts/pi-dev.sh` then `gk-pi <subcommand>`.
FROM ubuntu:25.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8

# Switch to legacy sources.list so adding arm64 via ports.ubuntu.com is a
# single-file append — simpler than editing the deb822-format default.
RUN rm -f /etc/apt/sources.list.d/ubuntu.sources && \
    printf '%s\n' \
      'deb [arch=amd64] http://archive.ubuntu.com/ubuntu plucky main restricted universe multiverse' \
      'deb [arch=amd64] http://archive.ubuntu.com/ubuntu plucky-updates main restricted universe multiverse' \
      'deb [arch=amd64] http://archive.ubuntu.com/ubuntu plucky-backports main restricted universe multiverse' \
      'deb [arch=amd64] http://security.ubuntu.com/ubuntu plucky-security main restricted universe multiverse' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports plucky main restricted universe multiverse' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports plucky-updates main restricted universe multiverse' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports plucky-backports main restricted universe multiverse' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports plucky-security main restricted universe multiverse' \
      > /etc/apt/sources.list && \
    dpkg --add-architecture arm64

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        gcc-15-aarch64-linux-gnu g++-15-aarch64-linux-gnu \
        cmake ninja-build pkg-config \
        python3 python3-pip \
        git curl rsync ca-certificates \
        glslc spirv-tools \
        libvulkan-dev:arm64 \
        libx11-dev:arm64 libxext-dev:arm64 libxrandr-dev:arm64 \
        libwayland-dev:arm64 wayland-protocols libxkbcommon-dev:arm64 && \
    ln -sf /usr/bin/aarch64-linux-gnu-gcc-15 /usr/local/bin/aarch64-linux-gnu-gcc && \
    ln -sf /usr/bin/aarch64-linux-gnu-g++-15 /usr/local/bin/aarch64-linux-gnu-g++ && \
    rm -rf /var/lib/apt/lists/*

ENV GECKO_PLATFORM_ID=Linux-aarch64 \
    CMAKE_TOOLCHAIN_FILE=/workspace/docker/aarch64-toolchain.cmake \
    PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
    PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig

WORKDIR /workspace
