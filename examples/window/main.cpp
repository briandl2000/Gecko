#include "gecko/gecko.h"
#include "gecko/platform/platform_events.h"

namespace {

constexpr gecko::Label ExampleLabel = gecko::MakeLabel("example.window");
bool Running = true;

void OnClose(void*, const gecko::EventMeta&, gecko::EventView) noexcept
{
  Running = false;
}

}  // namespace

int main()
{
  gecko::GeckoConfig config {};
  config.AppName = "Gecko Window Example";
  config.EnableGraphics = false;
  if (gecko::Initialize(config) != gecko::InitializeResult::Success)
    return 1;

  auto* windows = gecko::platform::GetWindows();
  gecko::platform::WindowDesc desc {};
  desc.Title = "Gecko Window Example";
  desc.Size = {960, 540};
  const gecko::platform::WindowHandle window = windows->CreateWindow(desc);
  if (!window.IsValid())
  {
    gecko::Shutdown();
    return 2;
  }

  gecko::EventSubscription close =
      gecko::SubscribeEvent(gecko::platform::events::WindowCloseRequested, OnClose, nullptr);
  const gecko::platform::DpiInfo dpi = windows->GetDpi(window);
  GECKO_INFO(ExampleLabel, "window opened at {} DPI ({}x scale)", dpi.Dpi, dpi.Scale);

  while (Running && windows->IsWindowAlive(window))
  {
    gecko::platform::PumpEvents();
    (void)gecko::DispatchEvents();
    GECKO_SLEEP_MS(1);
  }

  close.Reset();
  windows->DestroyWindow(window);
  gecko::Shutdown();
  return 0;
}
