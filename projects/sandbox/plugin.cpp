#include "gecko/gecko.h"
#include "gecko/platform/platform_events.h"
#include "sandbox/Shaders.generated.h"

namespace {

static_assert(sizeof(gecko::sandbox::shaders::SmokeCompute) > 0);

constexpr gecko::Label PluginLabel = gecko::MakeLabel("plugin.sandbox");

gecko::platform::WindowHandle g_Window {};
gecko::EventSubscription g_CloseSubscription {};
bool g_Running = false;

void OnCloseRequested(void*, const gecko::EventMeta&, gecko::EventView) noexcept
{
  g_Running = false;
}

bool PluginInitialize(const gecko::PluginContext&) noexcept
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
  GECKO_INFO(PluginLabel, "Sandbox initialized");
  return true;
}

bool PluginUpdate(const gecko::PluginFrame&) noexcept
{
  auto* windows = gecko::platform::GetWindows();
  if (!g_Running || windows == nullptr || !windows->IsWindowAlive(g_Window))
    return false;

  gecko::platform::PumpEvents();
  (void)gecko::DispatchEvents();

  GECKO_SLEEP_MS(1);
  return g_Running;
}

void PluginShutdown() noexcept
{
  g_CloseSubscription.Reset();
  if (auto* windows = gecko::platform::GetWindows(); windows != nullptr && g_Window.IsValid())
    windows->DestroyWindow(g_Window);
  g_Window = {};
  g_Running = false;
  GECKO_INFO(PluginLabel, "Sandbox shut down");
}

const gecko::PluginApi PluginApi {
    .Name = "Sandbox",
    .Initialize = PluginInitialize,
    .Update = PluginUpdate,
    .Shutdown = PluginShutdown,
};

}  // namespace

GECKO_PLUGIN_EXPORT const gecko::PluginApi* GeckoPlugin_GetApi() noexcept
{
  return &PluginApi;
}
