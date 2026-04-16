import os
import platform

# Normalize architecture names across platforms
_ARCH_MAP = {"AMD64": "x86_64", "ARM64": "aarch64"}
_arch = _ARCH_MAP.get(platform.machine(), platform.machine())

PLATFORM_ID = f"{platform.system()}-{_arch}"
BUILD_DIR = os.environ.get("GECKO_BUILD_DIR", f"out/build/{PLATFORM_ID}")
OUTPUT_DIR = os.environ.get("GECKO_OUTPUT_DIR", f"out/{PLATFORM_ID}")


def is_network_path(path: str) -> bool:
    """Check if a path is on a network/mapped drive (Windows only)."""
    if os.name != "nt":
        return False
    if path.startswith("\\\\") or path.startswith("//"):
        return True
    drive = os.path.splitdrive(path)[0]
    if drive:
        import ctypes
        return ctypes.windll.kernel32.GetDriveTypeW(drive + "\\") == 4
    return False
