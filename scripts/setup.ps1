# Gecko Development Environment (PowerShell)
# Can be dot-sourced or executed: . .\scripts\setup.ps1 or .\scripts\setup.ps1

# Get the repository root regardless of how script is invoked
if ($PSScriptRoot) {
    $script:RepoRoot = Split-Path -Parent $PSScriptRoot
} else {
    $script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}

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

# Configure CMake if not already done
if (-not (Test-Path "$script:RepoRoot/out/build")) {
    Write-Host "Configuring CMake..."
    
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
    
    cmake -S "$script:RepoRoot" -B "$script:RepoRoot/out/build" -G "Ninja Multi-Config"
}

Write-Host "Gecko dev environment ready. Commands: gk build, gk test, gk package"
