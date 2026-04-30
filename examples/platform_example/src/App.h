#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/memory.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/platform/input.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/standard_log_sinks.h>
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
  ::gecko::AllocatorScope m_AllocScope {m_Allocator};

  ::gecko::runtime::RuntimeModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  ExampleModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;
  ::std::optional<::gecko::runtime::StandardLogSinks> m_LogSinks;

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
