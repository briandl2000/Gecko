# Gecko Development Environment (PowerShell)
#
# Bootstraps a Visual Studio Developer environment so `cl.exe` (or
# `clang-cl.exe`) is on PATH, defines the `gk` function, and runs the
# initial CMake configure. CMake auto-detects the compiler -- pre-set
# $env:CC / $env:CXX before dot-sourcing this file if you want to pin
# a specific one.

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $script:RepoRoot) {
    $script:RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
    $script:RepoRoot = Split-Path -Parent $script:RepoRoot
}

# Find and load Visual Studio environment so cl.exe / clang-cl.exe are
# available. Skipped if the caller already has $env:VSINSTALLDIR set
# (e.g. running from a Developer PowerShell).
function Initialize-VSDev {
    if ($env:VSINSTALLDIR) {
        return $true
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Host "Warning: Visual Studio Installer not found." -ForegroundColor Yellow
        Write-Host "Install Visual Studio 2022 (or Build Tools) from https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
        return $false
    }

    $vsPath = & $vswhere -latest -products * -property installationPath
    if (-not $vsPath) {
        Write-Host "Warning: no Visual Studio installation found." -ForegroundColor Yellow
        return $false
    }

    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        Write-Host "Warning: vcvarsall.bat not found under $vsPath" -ForegroundColor Yellow
        return $false
    }

    Write-Host "Loading Visual Studio environment from: $vsPath" -ForegroundColor Cyan
    $arch = "x64"
    $cmd = "`"$vcvarsall`" $arch >nul 2>&1 && set"
    $output = cmd /c $cmd
    foreach ($line in $output) {
        if ($line -match "^([^=]+)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
    return $true
}

$script:VSDevReady = Initialize-VSDev

# Define the `gk` command.
$global:GeckoRepoRoot = $script:RepoRoot
function global:gk {
    python "$global:GeckoRepoRoot/scripts/cli.py" $args
}

# Platform identifier for build/output directory separation.
$arch = switch ($env:PROCESSOR_ARCHITECTURE) {
    "AMD64" { "x86_64" }
    "ARM64" { "aarch64" }
    default { $env:PROCESSOR_ARCHITECTURE }
}
$env:GECKO_PLATFORM_ID = "Windows-$arch"

$buildDir = "$script:RepoRoot\out\build\$env:GECKO_PLATFORM_ID"

if (-not $script:VSDevReady) {
    Write-Host "Skipping CMake configure: Visual Studio developer environment not available." -ForegroundColor Yellow
    Write-Host "Set up VS Build Tools (or pre-set `$env:CC / `$env:CXX) and re-run setup.ps1." -ForegroundColor Yellow
    return
}

if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
    Write-Host "Configuring CMake for $env:GECKO_PLATFORM_ID..."
    Write-Host "Build directory: $buildDir" -ForegroundColor Cyan

    cmake -S "$script:RepoRoot" -B "$buildDir" `
        -G "Ninja Multi-Config" `
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
        -DGECKO_BUILD_TESTS=ON
}

Write-Host "Gecko dev environment ready ($env:GECKO_PLATFORM_ID). Commands: gk build, gk test, gk package"
Write-Host "Build dir:  $buildDir" -ForegroundColor DarkGray
Write-Host "Output dir: $script:RepoRoot\out\$env:GECKO_PLATFORM_ID" -ForegroundColor DarkGray
