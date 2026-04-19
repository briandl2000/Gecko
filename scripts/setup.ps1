# Gecko Development Environment (PowerShell)
# DEPRECATED: Windows builds now use MSYS2 UCRT64 with MinGW-w64 GCC.
# Use the MSYS2 UCRT64 terminal and run: source scripts/setup.sh
# This script is kept for reference only and will be removed in a future release.
Write-Host "ERROR: This script is deprecated." -ForegroundColor Red
Write-Host "Windows builds now use MSYS2 UCRT64 with MinGW-w64 GCC." -ForegroundColor Yellow
Write-Host "Open the MSYS2 UCRT64 terminal and run: source scripts/setup.sh" -ForegroundColor Yellow
Write-Host "See docs/build.md for setup instructions." -ForegroundColor Yellow
return

# Store VS path globally for later use
$script:VSInstallPath = $null

# Function to find and load Visual Studio environment
function Initialize-VSDev {
    # Skip if already loaded
    if ($env:VSINSTALLDIR) {
        $script:VSInstallPath = $env:VSINSTALLDIR
        return $true
    }

    # Find vswhere
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Host "Warning: Could not find Visual Studio Installer." -ForegroundColor Yellow
        Write-Host "Install Visual Studio Build Tools from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
        return $false
    }

    # Get VS installation path (include Build Tools with -products *)
    $vsPath = & $vswhere -latest -products * -property installationPath
    if (-not $vsPath) {
        Write-Host "Warning: No Visual Studio installation found." -ForegroundColor Yellow
        return $false
    }
    
    $script:VSInstallPath = $vsPath

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

# Function to find clang-cl (standalone or VS bundled)
function Find-ClangCl {
    # First check if already in PATH
    $clangCl = Get-Command clang-cl -ErrorAction SilentlyContinue
    if ($clangCl) {
        return $clangCl.Source
    }
    
    # Check VS bundled clang-cl using the path we found earlier
    if ($script:VSInstallPath) {
        $vsClangPath = Join-Path $script:VSInstallPath "VC\Tools\Llvm\x64\bin\clang-cl.exe"
        if (Test-Path $vsClangPath) {
            return $vsClangPath
        }
    }
    
    # Also check via VSINSTALLDIR env var (might be set by vcvarsall)
    if ($env:VSINSTALLDIR) {
        $vsClangPath = Join-Path $env:VSINSTALLDIR "VC\Tools\Llvm\x64\bin\clang-cl.exe"
        if (Test-Path $vsClangPath) {
            return $vsClangPath
        }
    }
    
    return $null
}

# Load VS environment
Initialize-VSDev | Out-Null

# Store repo root in global variable for gk function
$global:GeckoRepoRoot = $script:RepoRoot

# Create gk function (doesn't pollute PATH)
function global:gk {
    python "$global:GeckoRepoRoot/scripts/cli.py" $args
}

# Platform identifier for build/output directory separation
# Normalize architecture names to match Linux conventions
$arch = switch ($env:PROCESSOR_ARCHITECTURE) {
    "AMD64" { "x86_64" }
    "ARM64" { "aarch64" }
    default { $env:PROCESSOR_ARCHITECTURE }
}
$env:GECKO_PLATFORM_ID = "Windows-$arch"

# Clear stale env vars from previous setups
Remove-Item Env:\GECKO_BUILD_DIR -ErrorAction SilentlyContinue
Remove-Item Env:\GECKO_OUTPUT_DIR -ErrorAction SilentlyContinue

# Build and output directories live in the project folder
$buildDir = "$script:RepoRoot\out\build\$env:GECKO_PLATFORM_ID"

# Find and configure compiler
$clangPath = Find-ClangCl

if ($clangPath) {
    Write-Host "Using Clang compiler: $clangPath" -ForegroundColor Green
    
    # Add to PATH if not already there
    $clangDir = Split-Path $clangPath
    if ($env:PATH -notlike "*$clangDir*") {
        $env:PATH = "$clangDir;$env:PATH"
    }
    
    $env:CC = "clang-cl"
    $env:CXX = "clang-cl"
} else {
    Write-Host ""
    Write-Host "ERROR: Clang compiler (clang-cl) not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Gecko requires the Clang compiler. To install via Visual Studio:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  1. Open 'Visual Studio Installer'" -ForegroundColor White
    Write-Host "  2. Click 'Modify' on your Build Tools or VS installation" -ForegroundColor White
    Write-Host "  3. Go to 'Individual Components' tab" -ForegroundColor White
    Write-Host "  4. Search for 'C++ Clang Compiler' and check it" -ForegroundColor White
    Write-Host "  5. Click 'Modify' to install" -ForegroundColor White
    Write-Host ""
    Write-Host "After installing, run this script again." -ForegroundColor Cyan
    return
}

# Configure CMake if not already done
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
