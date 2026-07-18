@echo off
setlocal EnableExtensions

set "Root=%~dp0"
set "Config=%~1"
if "%Config%"=="" set "Config=debug"
set "Action=%~2"
if "%Action%"=="" set "Action=build"

if /I "%Config%"=="debug" (
  set "ConfigName=Debug"
  set "ConfigFlags=/Od /Zi /D_DEBUG=1"
) else if /I "%Config%"=="release" (
  set "ConfigName=Release"
  set "ConfigFlags=/O2 /Zi /DNDEBUG=1"
) else (
  echo usage: build.bat [debug^|release] [build^|clean]
  exit /b 2
)

set "BuildDir=%Root%out\Windows-x86_64\handmade\%ConfigName%"
set "ObjectDir=%BuildDir%\obj"
set "BinaryDir=%BuildDir%\bin"

if /I "%Action%"=="clean" (
  if exist "%BuildDir%" rmdir /S /Q "%BuildDir%"
  exit /b 0
)

if /I not "%Action%"=="build" (
  echo unknown action: %Action%
  exit /b 2
)

where cl >nul 2>nul
if errorlevel 1 (
  echo cl.exe was not found. Run this from a Visual Studio Developer Command Prompt.
  exit /b 1
)

if not exist "%ObjectDir%" mkdir "%ObjectDir%"
if not exist "%BinaryDir%" mkdir "%BinaryDir%"

set "VulkanInclude="
set "VulkanLibrary=vulkan-1.lib"
if defined VULKAN_SDK (
  set "VulkanInclude=/I%VULKAN_SDK%\Include"
  set "VulkanLibrary=/LIBPATH:%VULKAN_SDK%\Lib vulkan-1.lib"
)

set Sources=^
 "%Root%src\gecko_engine.cpp"

echo Building Gecko %ConfigName%
cl /nologo /std:c++latest /MP /LD /W4 /WX /wd4201 /wd4324 ^
 %ConfigFlags% /DGECKO_BUILD_SHARED=1 /DGECKO_BUILDING=1 ^
 /DGECKO_PLATFORM_WINDOWS=1 /DGECKO_GRAPHICS_VULKAN=1 ^
 /DGECKO_GRAPHICS_VULKAN_WIN32=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" /I"%Root%src\core" /I"%Root%src\graphics" %VulkanInclude% ^
 %Sources% /Fo"%ObjectDir%\\" /Fe"%BinaryDir%\Gecko.dll" ^
 /link /IMPLIB:"%BinaryDir%\Gecko.lib" user32.lib shcore.lib ole32.lib winmm.lib %VulkanLibrary%
if errorlevel 1 exit /b 1

echo Building gecko_game.dll
cl /nologo /std:c++latest /LD /W4 /WX %ConfigFlags% ^
 /DGECKO_BUILD_SHARED=1 /DGECKO_PLATFORM_WINDOWS=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" "%Root%projects\sandbox\game.cpp" ^
 "%BinaryDir%\Gecko.lib" /Fe"%BinaryDir%\gecko_game.dll"
if errorlevel 1 exit /b 1

echo Building gecko_launcher
cl /nologo /std:c++latest /W4 /WX %ConfigFlags% ^
 /DGECKO_BUILD_SHARED=1 /DGECKO_PLATFORM_WINDOWS=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" "%Root%projects\launcher\main.cpp" ^
 "%BinaryDir%\Gecko.lib" /Fe"%BinaryDir%\gecko_launcher.exe"
if errorlevel 1 exit /b 1

echo Built %BinaryDir%\gecko_launcher.exe and %BinaryDir%\gecko_game.dll
