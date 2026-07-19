@echo off
setlocal EnableExtensions

set "Root=%~dp0"
set "Config=%~1"
if "%Config%"=="" set "Config=debug"
set "Action=%~2"
if "%Action%"=="" set "Action=build"

if /I "%Config%"=="debug" (
  set "ConfigName=Debug"
  set "ConfigFlags=/Od /Z7 /D_DEBUG=1"
) else if /I "%Config%"=="release" (
  set "ConfigName=Release"
  set "ConfigFlags=/O2 /Z7 /DNDEBUG=1"
) else (
  echo usage: build.bat [debug^|release] [build^|clean^|examples]
  exit /b 2
)

set "BuildDir=%Root%out\Windows-x86_64\%ConfigName%"
set "ObjectDir=%BuildDir%\obj"
set "GeneratedDir=%BuildDir%\generated"
set "BinaryDir=%BuildDir%\bin"

if /I "%Action%"=="clean" (
  echo [CLEAN] %BuildDir%
  if exist "%BuildDir%" rmdir /S /Q "%BuildDir%"
  exit /b 0
)
if /I not "%Action%"=="build" if /I not "%Action%"=="examples" (
  echo unknown action: %Action%
  exit /b 2
)

where cl >nul 2>nul
if errorlevel 1 (
  echo cl.exe was not found. Run from a Visual Studio Developer Command Prompt.
  exit /b 1
)

if not exist "%ObjectDir%" mkdir "%ObjectDir%"
if not exist "%GeneratedDir%" mkdir "%GeneratedDir%"
if not exist "%BinaryDir%" mkdir "%BinaryDir%"

set "VulkanInclude="
set "VulkanLibrary=vulkan-1.lib"
if defined VULKAN_SDK (
  set "VulkanInclude=/I"%VULKAN_SDK%\Include""
  set "VulkanLibrary=/LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib"
)

set "CommonFlags=/nologo /std:c++latest /W4 /WX /wd4201 /wd4324 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /DGECKO_BUILD_SHARED=1 /DGECKO_PLATFORM_WINDOWS=1 /DGECKO_GRAPHICS_VULKAN=1 /DGECKO_GRAPHICS_VULKAN_WIN32=1 /D_CRT_SECURE_NO_WARNINGS"

echo [GECKO] %ConfigName% %Action%
echo [CXX  ] Gecko.dll
cl %CommonFlags% /MP /LD %ConfigFlags% /DGECKO_BUILDING=1 /I"%Root%include" ^
 /I"%Root%src\core" /I"%Root%src\graphics" %VulkanInclude% ^
 "%Root%src\gecko_engine.cpp" /Fo"%ObjectDir%\" /Fe"%BinaryDir%\Gecko.dll" ^
 /link /IMPLIB:"%BinaryDir%\Gecko.lib" user32.lib shcore.lib ole32.lib winmm.lib %VulkanLibrary%
if errorlevel 1 exit /b 1

if /I "%Action%"=="examples" (
  if not exist "%BuildDir%\tools" mkdir "%BuildDir%\tools"
  echo [TOOL ] gecko_embed.exe
  cl /nologo /std:c++latest /O2 "%Root%tools\embed.cpp" /Fo"%ObjectDir%\gecko_embed.obj" /Fe"%BuildDir%\tools\gecko_embed.exe"
  if errorlevel 1 exit /b 1
  call "%Root%tools\build_shaders.bat" "%Root%examples\graphics_example\shaders" "%GeneratedDir%\graphics_example_shaders"
  if errorlevel 1 exit /b 1
  echo [EMBED] graphics_example\shaders.h
  "%BuildDir%\tools\gecko_embed.exe" "%GeneratedDir%\graphics_example_shaders\shaders.h" gecko::examples::graphics_example::shaders ^
    "%GeneratedDir%\graphics_example_shaders\triangle.vert.spv" TriangleVert ^
    "%GeneratedDir%\graphics_example_shaders\triangle.frag.spv" TriangleFrag ^
    "%GeneratedDir%\graphics_example_shaders\fullscreen.vert.spv" FullscreenVert ^
    "%GeneratedDir%\graphics_example_shaders\fullscreen.frag.spv" FullscreenFrag ^
    "%GeneratedDir%\graphics_example_shaders\plasma.comp.spv" PlasmaComp
  if errorlevel 1 exit /b 1
  call :BuildExample app_skeleton
  if errorlevel 1 exit /b 1
  call :BuildExample core_example
  if errorlevel 1 exit /b 1
  call :BuildExample math_example
  if errorlevel 1 exit /b 1
  call :BuildExample platform_example
  if errorlevel 1 exit /b 1
  call :BuildExample graphics_example
  if errorlevel 1 exit /b 1
  echo [DONE ] %BinaryDir%\gecko_example_*.exe
  exit /b 0
)

echo [LINK ] gecko_game.dll
cl %CommonFlags% /LD %ConfigFlags% /I"%Root%include" "%Root%projects\sandbox\game.cpp" ^
 "%BinaryDir%\Gecko.lib" /Fo"%ObjectDir%\game.obj" /Fe"%BinaryDir%\gecko_game.dll"
if errorlevel 1 exit /b 1

echo [LINK ] gecko_launcher.exe
cl %CommonFlags% %ConfigFlags% /I"%Root%include" "%Root%projects\launcher\main.cpp" ^
 "%BinaryDir%\Gecko.lib" /Fo"%ObjectDir%\launcher.obj" /Fe"%BinaryDir%\gecko_launcher.exe"
if errorlevel 1 exit /b 1

echo [DONE ] %BinaryDir%\gecko_launcher.exe
exit /b 0

:BuildExample
echo [LINK ] gecko_example_%~1.exe
if not exist "%ObjectDir%\examples\%~1" mkdir "%ObjectDir%\examples\%~1"
cl %CommonFlags% %ConfigFlags% /I"%Root%include" /I"%GeneratedDir%\graphics_example_shaders" "%Root%examples\%~1\src\*.cpp" ^
 "%BinaryDir%\Gecko.lib" /Fo"%ObjectDir%\examples\%~1\\" /Fe"%BinaryDir%\gecko_example_%~1.exe"
exit /b %errorlevel%
