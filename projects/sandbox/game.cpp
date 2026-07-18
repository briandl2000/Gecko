#include "gecko/gecko.h"
#include "gecko/platform/platform_events.h"

namespace {

constexpr gecko::Label GameLabel = gecko::MakeLabel("game.sandbox");

gecko::platform::WindowHandle g_Window {};
gecko::EventSubscription g_CloseSubscription {};
bool g_Running = false;

void OnCloseRequested(void*, const gecko::EventMeta&, gecko::EventView) noexcept
{
  g_Running = false;
}

bool GameInitialize(const gecko::GameContext&) noexcept
{
  auto* windows = gecko::platform::GetWindows();
  if (windows == nullptr)
    return false;

  gecko::platform::WindowDesc desc {};
  desc.Title = "Gecko Sandbox";
  desc.Size = {1280, 720};
  g_Window = windows->CreateWindow(desc);
  if (!g_Window.IsValid())
    return false;

  g_CloseSubscription = gecko::SubscribeEvent(gecko::platform::events::WindowCloseRequested, OnCloseRequested, nullptr);
  g_Running = true;
  GECKO_INFO(GameLabel, "Sandbox initialized");
  return true;
}

bool GameUpdate(const gecko::GameFrame&) noexcept
{
  auto* windows = gecko::platform::GetWindows();
  if (!g_Running || windows == nullptr || !windows->IsWindowAlive(g_Window))
    return false;

  gecko::platform::PumpEvents();
  (void)gecko::DispatchEvents();

  GECKO_SLEEP_MS(1);
  return g_Running;
}

void GameShutdown() noexcept
{
  g_CloseSubscription.Reset();
  if (auto* windows = gecko::platform::GetWindows(); windows != nullptr && g_Window.IsValid())
    windows->DestroyWindow(g_Window);
  g_Window = {};
  g_Running = false;
  GECKO_INFO(GameLabel, "Sandbox shut down");
}

const gecko::GameApi GameApi {
    .Name = "Sandbox",
    .Initialize = GameInitialize,
    .Update = GameUpdate,
    .Shutdown = GameShutdown,
};

}  // namespace

GECKO_GAME_EXPORT const gecko::GameApi* GeckoGame_GetApi() noexcept
{
  return &GameApi;
}
