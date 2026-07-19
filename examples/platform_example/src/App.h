#pragma once

#include "gecko/gecko.h"

namespace gecko::examples::platform_example {

class App
{
public:
  App() noexcept;
  ~App() noexcept;

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Initialized && m_MainWindow.IsValid();
  }

  int Run() noexcept;

private:
  struct InputWatch
  {
    platform::WindowHandle LastFocused {};
    platform::WindowHandle LastHovered {};
    bool Initialized {false};
  };

  void CreateMainWindow() noexcept;
  void SubscribeWindowEvents() noexcept;
  void RunFrame() noexcept;
  void PollInput() noexcept;
  void OnKey(platform::KeyCode key) noexcept;
  void HandleSpawnKey(platform::KeyCode key) noexcept;
  void HandleMainWindowKey(platform::KeyCode key) noexcept;
  void HandleTitleBarButtonsKey(platform::KeyCode key) noexcept;
  void HandleSizeConstraintsKey(platform::KeyCode key) noexcept;
  void PrintHelp() noexcept;
  void PrintWindowInfo() noexcept;
  void PrintInputSnapshot() noexcept;

  static constexpr usize MaxSpawnedWindows = 16;
  platform::WindowHandle m_MainWindow {};
  platform::WindowHandle m_SpawnedWindows[MaxSpawnedWindows] {};
  usize m_SpawnedWindowCount {0};
  bool m_Initialized {false};
  bool m_Running {true};
  InputWatch m_InputWatch {};

  EventSubscription m_CloseSub {};
  EventSubscription m_KeySub {};
  EventSubscription m_FocusSub {};
  EventSubscription m_MovedSub {};
  EventSubscription m_ResizedSub {};
  EventSubscription m_StateChangedSub {};
};

}  // namespace gecko::examples::platform_example
