from __future__ import annotations

import os
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def register(subparsers) -> None:
    parser = subparsers.add_parser(
        "package",
        help="Create release package",
    )
    parser.add_argument(
        "type",
        nargs="?",
        default="local",
        choices=["local", "dev", "release"],
        help="Package type: local (default), dev (timestamped), release",
    )
    parser.add_argument(
        "--config",
        default="release",
        choices=["debug", "release"],
        help="Build configuration",
    )
    parser.set_defaults(handler=_run)


def _get_version(repo_root: Path) -> tuple[str, str]:
    """Read version from CMakeLists.txt"""
    cmake = repo_root / "CMakeLists.txt"
    content = cmake.read_text()
    
    # Extract version
    import re
    version_match = re.search(r'project\(Gecko VERSION (\d+\.\d+\.\d+)', content)
    version = version_match.group(1) if version_match else "0.0.0"
    
    # Extract prerelease
    prerelease_match = re.search(r'set\(GECKO_VERSION_PRERELEASE "([^"]*)"\)', content)
    prerelease = prerelease_match.group(1) if prerelease_match else ""
    
    return version, prerelease


def _run(args) -> int:
    repo_root = Path(__file__).resolve().parents[2]
    config = "Release" if args.config == "release" else "Debug"
    
    version, prerelease = _get_version(repo_root)
    
    # Build version string
    if args.type == "dev":
        timestamp = datetime.utcnow().strftime("%Y%m%d%H%M%S")
        if prerelease:
            version_str = f"{version}-{prerelease}-dev.{timestamp}"
        else:
            version_str = f"{version}-dev.{timestamp}"
    elif args.type == "release":
        if prerelease:
            version_str = f"{version}-{prerelease}"
        else:
            version_str = version
    else:  # local
        version_str = f"{version}-local"
    
    package_name = f"gecko-{version_str}"
    install_dir = repo_root / "out" / "package" / package_name
    
    # Clean previous
    if install_dir.exists():
        shutil.rmtree(install_dir)
    
    # Build
    print(f"Building ({config})...")
    build_result = subprocess.run(
        ["cmake", "--build", "out/build", "--config", config],
        cwd=repo_root,
        check=False,
    )
    if build_result.returncode != 0:
        return build_result.returncode
    
    # Install to package directory
    print(f"Creating package: {package_name}")
    install_result = subprocess.run(
        ["cmake", "--install", "out/build", "--prefix", str(install_dir), "--config", config],
        cwd=repo_root,
        check=False,
    )
    if install_result.returncode != 0:
        return install_result.returncode
    
    # Create archive
    archive_path = repo_root / "out" / "package" / f"{package_name}.tar.gz"
    print(f"Creating archive: {archive_path.name}")
    
    import tarfile
    with tarfile.open(archive_path, "w:gz") as tar:
        tar.add(install_dir, arcname=package_name)
    
    print(f"\nPackage created: out/package/{archive_path.name}")
    print(f"Install directory: out/package/{package_name}/")
    
    return 0
