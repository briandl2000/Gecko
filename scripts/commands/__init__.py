import os
import platform
from pathlib import Path

# Normalize architecture names across platforms
_ARCH_MAP = {"AMD64": "x86_64", "ARM64": "aarch64"}
_arch = _ARCH_MAP.get(platform.machine(), platform.machine())

# Repo root derived from this file's location (scripts/commands/__init__.py)
# Use absolute() not resolve() — resolve() converts mapped drives to UNC paths
# which cmd.exe cannot use as working directories.
_REPO_ROOT = str(Path(__file__).absolute().parents[2])

PLATFORM_ID = f"{platform.system()}-{_arch}"
BUILD_DIR = os.environ.get("GECKO_BUILD_DIR", os.path.join(_REPO_ROOT, "out", "build", PLATFORM_ID))
OUTPUT_DIR = os.environ.get("GECKO_OUTPUT_DIR", os.path.join(_REPO_ROOT, "out", PLATFORM_ID))


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
