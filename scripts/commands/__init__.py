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

def _is_windows() -> bool:
    """Check if running on Windows (including MSYS2/MinGW environments)."""
    s = platform.system()
    return s == "Windows" or s.startswith("MINGW") or s.startswith("MSYS")

# Normalize platform name so MINGW64_NT-* becomes Windows
_platform_name = "Windows" if _is_windows() else platform.system()
PLATFORM_ID = os.environ.get("GECKO_PLATFORM_ID", f"{_platform_name}-{_arch}")
BUILD_DIR = os.environ.get("GECKO_BUILD_DIR", os.path.join(_REPO_ROOT, "out", "build", PLATFORM_ID))
OUTPUT_DIR = os.environ.get("GECKO_OUTPUT_DIR", os.path.join(_REPO_ROOT, "out", PLATFORM_ID))


def is_network_path(path: str) -> bool:
    """Check if a path is on a network/mapped drive (Windows only)."""
    if not _is_windows():
        return False
    if path.startswith("\\\\") or path.startswith("//"):
        return True
    # In MSYS2, paths like /z/foo map to Z:\foo — extract the drive letter
    import re
    msys_match = re.match(r"^/([a-zA-Z])/", path)
    if msys_match:
        drive = msys_match.group(1).upper() + ":"
    else:
        drive = os.path.splitdrive(path)[0]
    if drive:
        try:
            import ctypes
            return ctypes.windll.kernel32.GetDriveTypeW(drive + "\\") == 4
        except AttributeError:
            # MSYS2 Python lacks ctypes.windll — use 'net use' to check
            import subprocess
            try:
                result = subprocess.run(
                    ["net", "use", drive],
                    capture_output=True, text=True, check=False,
                )
                return result.returncode == 0
            except FileNotFoundError:
                return False
    return False
