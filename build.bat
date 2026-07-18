@echo off
setlocal EnableExtensions

set "Root=%~dp0"
set "Config=%~1"
if "%Config%"=="" set "Config=debug"
set "Action=%~2"
if "%Action%"=="" set "Action=build"
set "GameProject=%GECKO_GAME%"
if "%GameProject%"=="" set "GameProject=sandbox"

if /I "%Config%"=="debug" (
  set "ConfigName=Debug"
  set "ConfigFlags=/Od /Zi /D_DEBUG=1"
) else if /I "%Config%"=="release" (
  set "ConfigName=Release"
  set "ConfigFlags=/O2 /Zi /DNDEBUG=1"
) else (
  echo usage: build.bat [debug^|release] [build^|clean^|monolithic^|examples]
  exit /b 2
)

set "BuildDir=%Root%out\Windows-x86_64\handmade\%ConfigName%"
set "ObjectDir=%BuildDir%\obj"
set "BinaryDir=%BuildDir%\bin"
set "GameSource=%Root%projects\%GameProject%\game.cpp"

if /I "%Action%"=="clean" (
  if exist "%BuildDir%" rmdir /S /Q "%BuildDir%"
  exit /b 0
)

if /I not "%Action%"=="build" (
  if /I not "%Action%"=="monolithic" (
    if /I not "%Action%"=="examples" (
      echo unknown action: %Action%
      exit /b 2
    )
  )
)

if /I not "%Action%"=="examples" if not exist "%GameSource%" (
  echo game project not found: projects\%GameProject%\game.cpp
  exit /b 1
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
if /I "%Action%"=="monolithic" (
  cl /nologo /std:c++latest /MP /W4 /WX /wd4201 /wd4324 /EHs-c- /GR- ^
   %ConfigFlags% /D_HAS_EXCEPTIONS=0 /DGECKO_BUILD_SHARED=0 /DGECKO_BUILDING=1 ^
   /DGECKO_MONOLITHIC_GAME=1 /DGECKO_PLATFORM_WINDOWS=1 /DGECKO_GRAPHICS_VULKAN=1 ^
   /DGECKO_GRAPHICS_VULKAN_WIN32=1 /D_CRT_SECURE_NO_WARNINGS ^
   /I"%Root%include" /I"%Root%src\core" /I"%Root%src\graphics" %VulkanInclude% ^
   %Sources% "%GameSource%" "%Root%projects\launcher\main.cpp" ^
   /Fe"%BinaryDir%\gecko_monolithic.exe" ^
   /link user32.lib shcore.lib ole32.lib winmm.lib %VulkanLibrary%
  if errorlevel 1 exit /b 1
  echo Built %BinaryDir%\gecko_monolithic.exe
  exit /b 0
)

cl /nologo /std:c++latest /MP /LD /W4 /WX /wd4201 /wd4324 /EHs-c- /GR- ^
 %ConfigFlags% /DGECKO_BUILD_SHARED=1 /DGECKO_BUILDING=1 ^
 /D_HAS_EXCEPTIONS=0 ^
 /DGECKO_PLATFORM_WINDOWS=1 /DGECKO_GRAPHICS_VULKAN=1 ^
 /DGECKO_GRAPHICS_VULKAN_WIN32=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" /I"%Root%src\core" /I"%Root%src\graphics" %VulkanInclude% ^
 %Sources% /Fo"%ObjectDir%\\" /Fe"%BinaryDir%\Gecko.dll" ^
 /link /IMPLIB:"%BinaryDir%\Gecko.lib" user32.lib shcore.lib ole32.lib winmm.lib %VulkanLibrary%
if errorlevel 1 exit /b 1

if /I "%Action%"=="examples" (
  call "%Root%tools\build_shaders.bat" "%Root%examples\triangle\shaders" "%BinaryDir%\shaders"
  if errorlevel 1 exit /b 1
  for %%E in (core window triangle) do (
    echo Building gecko_example_%%E
    cl /nologo /std:c++latest /W4 /WX /EHs-c- /GR- %ConfigFlags% ^
     /D_HAS_EXCEPTIONS=0 /DGECKO_BUILD_SHARED=1 /DGECKO_PLATFORM_WINDOWS=1 ^
     /DGECKO_GRAPHICS_VULKAN=1 /DGECKO_GRAPHICS_VULKAN_WIN32=1 /D_CRT_SECURE_NO_WARNINGS ^
     /I"%Root%include" "%Root%examples\%%E\main.cpp" ^
     "%BinaryDir%\Gecko.lib" /Fe"%BinaryDir%\gecko_example_%%E.exe"
    if errorlevel 1 exit /b 1
  )
  echo Built Gecko examples in %BinaryDir%
  exit /b 0
)

echo Building gecko_game.dll
if exist "%Root%projects\%GameProject%\shaders" (
  dir /S /B "%Root%projects\%GameProject%\shaders\*.hlsl" >nul 2>nul
  if not errorlevel 1 (
    call "%Root%tools\build_shaders.bat" "%Root%projects\%GameProject%\shaders" "%BinaryDir%\shaders"
    if errorlevel 1 exit /b 1
  )
)
cl /nologo /std:c++latest /LD /W4 /WX /EHs-c- /GR- %ConfigFlags% ^
 /D_HAS_EXCEPTIONS=0 ^
 /DGECKO_BUILD_SHARED=1 /DGECKO_PLATFORM_WINDOWS=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" "%GameSource%" ^
 "%BinaryDir%\Gecko.lib" /Fe"%BinaryDir%\gecko_game.dll"
if errorlevel 1 exit /b 1

echo Building gecko_launcher
cl /nologo /std:c++latest /W4 /WX /EHs-c- /GR- %ConfigFlags% ^
 /D_HAS_EXCEPTIONS=0 ^
 /DGECKO_BUILD_SHARED=1 /DGECKO_PLATFORM_WINDOWS=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" "%Root%projects\launcher\main.cpp" ^
 "%BinaryDir%\Gecko.lib" /Fe"%BinaryDir%\gecko_launcher.exe"
if errorlevel 1 exit /b 1

echo Built %BinaryDir%\gecko_launcher.exe and %BinaryDir%\gecko_game.dll ^(%GameProject%^)
