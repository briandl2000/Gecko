#pragma once

#include "gecko/gecko.h"

namespace gecko::examples::app_skeleton {

struct AppConfig
{
  const char* Title {"Gecko App"};
  bool Windowed {true};
  u32 MaxFrames {0};
  platform::DisplayBackendKind Backend {platform::DisplayBackendKind::Auto};
};

class App
{
public:
  explicit App(const AppConfig& config) noexcept;
  ~App() noexcept;

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Initialized;
  }

  int Run() noexcept;

private:
  void RunHeadless() noexcept;
  void RunWindowed() noexcept;

  AppConfig m_Config {};
  bool m_Initialized {false};
};

}  // namespace gecko::examples::app_skeleton
