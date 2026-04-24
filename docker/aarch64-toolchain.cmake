# Gecko: aarch64 cross-compile toolchain used inside the docker/aarch64 image.
#
# Activated by setting CMAKE_TOOLCHAIN_FILE to this file inside the
# container. Not intended for host-side use: paths below assume the multi-arch
# layout that the Dockerfile provisions.

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Dev libs for arm64 live under /usr/lib/aarch64-linux-gnu thanks to multi-arch.
# Headers are shared with amd64 packages (same /usr/include) — no sysroot needed.
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

# Look for libraries and packages only in the aarch64 multi-arch location so
# we don't accidentally link amd64 runtime libs.
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu /usr /)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
