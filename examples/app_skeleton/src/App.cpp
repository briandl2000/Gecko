#include "App.h"

#include "gecko/platform/platform_events.h"

namespace gecko::examples::app_skeleton {

namespace {

constexpr Label MainLabel = MakeLabel("example.app_skeleton");

}  // namespace

App::App(const AppConfig& config) noexcept : m_Config(config)
{
  GeckoConfig engineConfig {};
  engineConfig.AppName = config.Title;
  engineConfig.Platform.Backend = config.Backend;
  engineConfig.EnableGraphics = false;
  m_Initialized = Initialize(engineConfig) == InitializeResult::Success;
  if (m_Initialized)
    GECKO_INFO(MainLabel, "Gecko {}", VersionFullString());
}

App::~App() noexcept
{
  if (m_Initialized)
    Shutdown();
}

int App::Run() noexcept
{
  if (!IsValid())
    return 1;

  if (m_Config.Windowed)
    RunWindowed();
  else
    RunHeadless();
  return 0;
}

void App::RunHeadless() noexcept
{
  GECKO_INFO(MainLabel, "running headless");
  GECKO_SLEEP_MS(10);
}

void App::RunWindowed() noexcept
{
  auto* windows = platform::GetWindows();
  platform::WindowDesc desc {};
  desc.Title = m_Config.Title;
  desc.Size = {1280, 720};
  const platform::WindowHandle window = windows->CreateWindow(desc);
  if (!window.IsValid())
  {
    GECKO_ERROR(MainLabel, "failed to create window");
    return;
  }

  bool running = true;
  u32 frames = 0;
  EventSubscription close = SubscribeEvent(
      platform::events::WindowCloseRequested,
      [](void* user, const EventMeta&, EventView) noexcept { *static_cast<bool*>(user) = false; }, &running);

  while (running && windows->IsWindowAlive(window))
  {
    GECKO_PROFILE_NAMED(MainLabel, "Frame");
    platform::PumpEvents();
    (void)DispatchEvents();
    GECKO_SLEEP_MS(16);

    ++frames;
    if (m_Config.MaxFrames != 0 && frames >= m_Config.MaxFrames)
      running = false;
  }

  close.Reset();
  windows->DestroyWindow(window);
}

}  // namespace gecko::examples::app_skeleton
