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
 "%Root%src\core\services.cpp" ^
 "%Root%src\core\services\engine.cpp" ^
 "%Root%src\core\services\events.cpp" ^
 "%Root%src\core\services\jobs.cpp" ^
 "%Root%src\core\services\log.cpp" ^
 "%Root%src\core\services\memory.cpp" ^
 "%Root%src\core\services\modules.cpp" ^
 "%Root%src\core\services\module_registry.cpp" ^
 "%Root%src\core\services\profiler.cpp" ^
 "%Root%src\core\utility\random.cpp" ^
 "%Root%src\core\utility\thread.cpp" ^
 "%Root%src\core\utility\time.cpp" ^
 "%Root%src\core\overwrite_new.cpp" ^
 "%Root%src\platform\clipboard.cpp" ^
 "%Root%src\platform\input.cpp" ^
 "%Root%src\platform\monitors_interface.cpp" ^
 "%Root%src\platform\platform_config.cpp" ^
 "%Root%src\platform\platform_io.cpp" ^
 "%Root%src\platform\platform_module.cpp" ^
 "%Root%src\platform\terminal.cpp" ^
 "%Root%src\platform\window_event_input.cpp" ^
 "%Root%src\platform\windows_interface.cpp" ^
 "%Root%src\platform\private\null_monitors_backend.cpp" ^
 "%Root%src\platform\private\null_windows_interface.cpp" ^
 "%Root%src\platform\win32\platform_io_win32.cpp" ^
 "%Root%src\platform\win32\threading_win32.cpp" ^
 "%Root%src\platform\win32\win32_monitors_backend.cpp" ^
 "%Root%src\platform\win32\win32_windows_backend.cpp" ^
 "%Root%src\runtime\async_trace_profiler_sink.cpp" ^
 "%Root%src\runtime\console_log_sink.cpp" ^
 "%Root%src\runtime\crash_safe_trace_profiler_sink.cpp" ^
 "%Root%src\runtime\event_bus.cpp" ^
 "%Root%src\runtime\file_log_sink.cpp" ^
 "%Root%src\runtime\immediate_logger.cpp" ^
 "%Root%src\runtime\ring_logger.cpp" ^
 "%Root%src\runtime\ring_profiler.cpp" ^
 "%Root%src\runtime\runtime_module.cpp" ^
 "%Root%src\runtime\standard_log_sinks.cpp" ^
 "%Root%src\runtime\thread_pool_job_system.cpp" ^
 "%Root%src\runtime\trace_file_sink.cpp" ^
 "%Root%src\runtime\trace_writer.cpp" ^
 "%Root%src\runtime\tracking_allocator.cpp" ^
 "%Root%src\graphics\graphics_device.cpp" ^
 "%Root%src\graphics\graphics_module.cpp" ^
 "%Root%src\graphics\private\null_device.cpp" ^
 "%Root%src\graphics\vulkan\vulkan_command_list.cpp" ^
 "%Root%src\graphics\vulkan\vulkan_device.cpp" ^
 "%Root%src\graphics\vulkan\vulkan_gpu_sampler.cpp" ^
 "%Root%src\graphics\vulkan\vulkan_surface.cpp" ^
 "%Root%src\graphics\vulkan\win32\vulkan_win32_surface.cpp"

echo Building Gecko %ConfigName%
cl /nologo /std:c++latest /MP /LD /W4 /WX /wd4201 /wd4324 ^
 %ConfigFlags% /DGECKO_BUILD_SHARED=1 /DGECKO_BUILDING=1 ^
 /DGECKO_PLATFORM_WINDOWS=1 /DGECKO_GRAPHICS_VULKAN=1 ^
 /DGECKO_GRAPHICS_VULKAN_WIN32=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" /I"%Root%src\core" /I"%Root%src\graphics" %VulkanInclude% ^
 %Sources% /Fo"%ObjectDir%\\" /Fe"%BinaryDir%\Gecko.dll" ^
 /link /IMPLIB:"%BinaryDir%\Gecko.lib" user32.lib shcore.lib ole32.lib winmm.lib %VulkanLibrary%
if errorlevel 1 exit /b 1

echo Building gecko_sandbox
cl /nologo /std:c++latest /W4 /WX %ConfigFlags% ^
 /DGECKO_BUILD_SHARED=1 /DGECKO_PLATFORM_WINDOWS=1 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%Root%include" ^
 "%Root%examples\app_skeleton\src\main.cpp" ^
 "%Root%examples\app_skeleton\src\App.cpp" ^
 "%BinaryDir%\Gecko.lib" /Fe"%BinaryDir%\gecko_sandbox.exe"
if errorlevel 1 exit /b 1

echo Built %BinaryDir%\gecko_sandbox.exe
