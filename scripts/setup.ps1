# Gecko Development Environment (PowerShell)
# Can be dot-sourced or executed: . .\scripts\setup.ps1 or .\scripts\setup.ps1

# Get the repository root regardless of how script is invoked
if ($PSScriptRoot) {
    $script:RepoRoot = Split-Path -Parent $PSScriptRoot
} else {
    $script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}

# Function to find and load Visual Studio environment
function Initialize-VSDev {
    # Skip if already loaded
    if ($env:VSINSTALLDIR) {
        return $true
    }

    # Find vswhere
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Host "Warning: Could not find Visual Studio installation." -ForegroundColor Yellow
        return $false
    }

    # Get VS installation path (include Build Tools with -products *)
    $vsPath = & $vswhere -latest -products * -property installationPath
    if (-not $vsPath) {
        Write-Host "Warning: No Visual Studio installation found." -ForegroundColor Yellow
        return $false
    }

    # Find vcvarsall.bat
    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        Write-Host "Warning: Could not find vcvarsall.bat" -ForegroundColor Yellow
        return $false
    }

    Write-Host "Loading Visual Studio environment from: $vsPath" -ForegroundColor Cyan

    # Run vcvarsall and capture environment variables
    $arch = "x64"
    $cmd = "`"$vcvarsall`" $arch >nul 2>&1 && set"
    $output = cmd /c $cmd

    # Parse and apply environment variables
    foreach ($line in $output) {
        if ($line -match "^([^=]+)=(.*)$") {
            $name = $matches[1]
            $value = $matches[2]
            [Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }

    Write-Host "Visual Studio environment loaded successfully!" -ForegroundColor Green
    return $true
}

# Load VS environment
Initialize-VSDev | Out-Null

# Store repo root in global variable for gk function
$global:GeckoRepoRoot = $script:RepoRoot

# Create gk function (doesn't pollute PATH)
function global:gk {
    python "$global:GeckoRepoRoot/scripts/cli.py" $args
}

# Configure CMake if not already done
if (-not (Test-Path "$script:RepoRoot/out/build")) {
    Write-Host "Configuring CMake..."
    cmake -S "$script:RepoRoot" -B "$script:RepoRoot/out/build" -G "Ninja Multi-Config"
}

Write-Host "Gecko dev environment ready. Commands: gk build, gk test, gk package"
