@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~2"=="" (
  echo usage: tools\build_shaders.bat ^<source-dir^> ^<output-dir^>
  exit /b 2
)

set "SourceDir=%~f1"
set "OutputDir=%~f2"
set "Compiler=%GLSLC%"
if not defined Compiler if defined VULKAN_SDK set "Compiler=%VULKAN_SDK%\Bin\glslc.exe"
if not defined Compiler set "Compiler=glslc.exe"

where "%Compiler%" >nul 2>nul
if errorlevel 1 (
  if not exist "%Compiler%" (
    echo glslc was not found; install the Vulkan SDK or set GLSLC
    exit /b 1
  )
)

if not exist "%OutputDir%" mkdir "%OutputDir%"
set "Found=0"
for /R "%SourceDir%" %%F in (*.hlsl) do (
  set "Stage="
  set "File=%%~nxF"
  if /I "!File:~-10!"==".vert.hlsl" set "Stage=vert"
  if /I "!File:~-10!"==".frag.hlsl" set "Stage=frag"
  if /I "!File:~-10!"==".comp.hlsl" set "Stage=comp"
  if /I "!File:~-10!"==".geom.hlsl" set "Stage=geom"
  if defined Stage (
    set "Found=1"
    set "Stem=%%~nF"
    echo [GLSLC] %%~nxF
    "%Compiler%" -x hlsl -fshader-stage=!Stage! -fentry-point=main -I"%SourceDir%" "%%~fF" -o "%OutputDir%\!Stem!.spv"
    if errorlevel 1 exit /b 1
  )
)

if "%Found%"=="0" (
  echo no .hlsl shaders found in %SourceDir%
  exit /b 1
)
