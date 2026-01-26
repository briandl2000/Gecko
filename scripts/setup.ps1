# Gecko Development Environment (PowerShell)
# Dot-source this file: . .\scripts\setup.ps1

$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

# Create gk function (doesn't pollute PATH)
function global:gk {
    python3 "$script:RepoRoot/scripts/cli.py" $args
}

# Configure CMake if not already done
if (-not (Test-Path "$script:RepoRoot/out/build")) {
    Write-Host "Configuring CMake..."
    cmake --preset debug -S $script:RepoRoot
}

Write-Host "Gecko dev environment ready. Commands: gk build, gk test, gk package"
