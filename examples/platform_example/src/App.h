#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/platform/input.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/file_log_sink.h>
#include <gecko/runtime/immediate_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>
#include <vector>

namespace gecko::examples::platform_example {

/// Interactive Window/Input API showcase.
///
/// Owns every service implementation, every module, and the main window.
/// `Run()` enters the main pump loop and exits when the user closes the
/// main window or presses Escape.
///
/// The big keyboard switch is split into grouped methods named after
/// what they do (window spawning, main-window manipulation, title-bar
/// buttons, size constraints, info printers).
class App
{
public:
  App();
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value() && m_MainWindow.IsValid();
  }

  /// Enters the main loop. Returns the process exit code.
  int Run();

private:
  struct AllocatorInstaller
  {
    explicit AllocatorInstaller(::gecko::IAllocator* a) noexcept;
    ~AllocatorInstaller();
    AllocatorInstaller(const AllocatorInstaller&) = delete;
    AllocatorInstaller& operator=(const AllocatorInstaller&) = delete;
    bool Ok = false;
  };

  class ExampleModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  /// Per-frame snapshot of the input service used to log only edge changes
  /// (focus / hover transitions, button presses, scroll deltas).
  struct InputWatch
  {
    ::gecko::platform::WindowHandle LastFocused {};
    ::gecko::platform::WindowHandle LastHovered {};
    bool Initialized = false;
  };

  void AttachSinks();
  void DetachSinks();
  void CreateMainWindow();
  void SubscribeWindowEvents();
  void RunFrame();
  void PollInput();

  // Key action groups.
  void OnKey(::gecko::platform::KeyCode key);
  void HandleSpawnKey(::gecko::platform::KeyCode key);
  void HandleMainWindowKey(::gecko::platform::KeyCode key);
  void HandleTitleBarButtonsKey(::gecko::platform::KeyCode key);
  void HandleSizeConstraintsKey(::gecko::platform::KeyCode key);
  void PrintHelp();
  void PrintWindowInfo();
  void PrintInputSnapshot();

  // Services / modules / sinks (declaration order = boot order).
  ::gecko::runtime::TrackingAllocator m_Allocator;
  AllocatorInstaller m_AllocatorInstaller {&m_Allocator};

  ::gecko::runtime::ThreadPoolJobSystem m_JobSystem;
  ::gecko::runtime::RingProfiler m_Profiler {1 << 16};
  ::gecko::runtime::ImmediateLogger m_Logger;
  ::gecko::runtime::EventBus m_EventBus;

  ::gecko::runtime::CoreServicesModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  ExampleModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;

  ::gecko::runtime::ConsoleLogSink m_ConsoleSink;
  ::gecko::runtime::FileLogSink m_FileSink {"log.txt"};
  bool m_SinksAttached = false;

  // Application state.
  ::gecko::platform::WindowHandle m_MainWindow {};
  ::std::vector<::gecko::platform::WindowHandle> m_SpawnedWindows;
  bool m_Running = true;
  InputWatch m_InputWatch;

  // Event subscriptions kept alive for the duration of Run().
  ::gecko::EventSubscription m_CloseSub {};
  ::gecko::EventSubscription m_KeySub {};
  ::gecko::EventSubscription m_FocusSub {};
  ::gecko::EventSubscription m_MovedSub {};
  ::gecko::EventSubscription m_ResizedSub {};
  ::gecko::EventSubscription m_StateChangedSub {};
};

}  // namespace gecko::examples::platform_example
