#include "gecko/gecko.h"
#include "gecko/platform/platform_events.h"
#include "sandbox/Shaders.generated.h"

namespace {

static_assert(sizeof(gecko::sandbox::shaders::SmokeCompute) > 0);

constexpr gecko::Label PluginLabel = gecko::MakeLabel("plugin.sandbox");

gecko::platform::WindowHandle g_Window {};
gecko::graphics::Swapchain g_Swapchain {};
gecko::EventSubscription g_CloseSubscription {};
gecko::EventSubscription g_ResizeSubscription {};
bool g_ResizePending = false;
bool g_Headless = false;
bool g_Running = false;

void OnCloseRequested(void*, const gecko::EventMeta&, gecko::EventView) noexcept
{
  g_Running = false;
}

void OnWindowResized(void*, const gecko::EventMeta&, gecko::EventView view) noexcept
{
  if (view.Size != sizeof(gecko::platform::events::WindowResizedPayload))
    return;
  const auto& event = *static_cast<const gecko::platform::events::WindowResizedPayload*>(view.Data());
  if (event.Window == g_Window && event.Width != 0 && event.Height != 0)
    g_ResizePending = true;
}

bool CreateGraphicsResources() noexcept
{
  auto* windows = gecko::platform::GetWindows();
  auto* device = gecko::graphics::GetGraphicsDevice();
  if (windows == nullptr || device == nullptr)
    return false;

  const gecko::platform::NativeWindowHandle native = windows->GetNativeWindowHandle(g_Window);
  if (native.Handle == nullptr && native.Display == nullptr)
  {
    g_Headless = true;
    return true;
  }

  const gecko::platform::Extent2D size = windows->GetClientSize(g_Window);
  gecko::graphics::SwapchainDesc desc {
      .Width = size.Width,
      .Height = size.Height,
      .Format = gecko::graphics::DataFormat::R8G8B8A8_UNORM,
      .DebugName = "Sandbox swapchain",
  };
  g_Swapchain = device->CreateSwapchain(native, desc);
  if (!g_Swapchain.IsValid())
    return false;
  return gecko::debug_renderer::Initialize(g_Swapchain.Desc.Format);
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
  if (!g_Window.IsValid() || !CreateGraphicsResources())
    return false;

  g_CloseSubscription = gecko::SubscribeEvent(gecko::platform::events::WindowCloseRequested, OnCloseRequested, nullptr);
  g_ResizeSubscription = gecko::SubscribeEvent(gecko::platform::events::WindowResized, OnWindowResized, nullptr);
  g_Running = true;
  GECKO_INFO(PluginLabel, "Sandbox initialized ({})", g_Headless ? "headless" : "Vulkan");
  return true;
}

void DrawSandboxLines(gecko::u64 frameIndex, gecko::u32 width, gecko::u32 height) noexcept
{
  const gecko::f32 w = static_cast<gecko::f32>(width);
  const gecko::f32 h = static_cast<gecko::f32>(height);
  const gecko::f32 travel = w > 240.0F ? w - 240.0F : 1.0F;
  const gecko::f32 x = 80.0F + static_cast<gecko::f32>(frameIndex % static_cast<gecko::u64>(travel));

  gecko::debug_renderer::DrawLine({40.0F, 40.0F}, {w - 40.0F, 40.0F}, {0.15F, 0.70F, 1.0F}, 3.0F);
  gecko::debug_renderer::DrawLine({40.0F, h - 40.0F}, {w - 40.0F, h - 40.0F}, {0.15F, 0.70F, 1.0F}, 3.0F);
  gecko::debug_renderer::DrawLine({x, h * 0.35F}, {x + 120.0F, h * 0.65F}, {1.0F, 0.75F, 0.15F}, 8.0F);
  gecko::debug_renderer::DrawLine({x + 120.0F, h * 0.35F}, {x, h * 0.65F}, {1.0F, 0.30F, 0.20F}, 8.0F);
}

bool RenderFrame(gecko::u64 frameIndex) noexcept
{
  auto* device = gecko::graphics::GetGraphicsDevice();
  if (device == nullptr)
    return false;
  if (g_ResizePending)
  {
    device->ResizeSwapchain(g_Swapchain);
    g_ResizePending = false;
  }

  const gecko::graphics::FrameContext frame = device->BeginFrame(g_Swapchain);
  if (!frame.Valid)
    return true;
  auto commandList = device->CreateGraphicsCommandList();
  if (!commandList || !commandList->IsValid())
    return false;

  const gecko::graphics::ClearValue clear = gecko::graphics::ClearValue::RenderTarget(0.015F, 0.025F, 0.05F, 1.0F);
  const gecko::graphics::RenderTarget targets[] = {frame.BackBuffer};
  const gecko::graphics::ClearValue clears[] = {clear};
  const gecko::graphics::BeginRenderingInfo rendering {
      .Colors = targets,
      .ClearColors = clears,
  };

  gecko::debug_renderer::BeginFrame();
  DrawSandboxLines(frameIndex, frame.BackBuffer.Desc.Width, frame.BackBuffer.Desc.Height);
  commandList->Begin();
  commandList->BeginRendering(rendering);
  gecko::debug_renderer::Submit(*commandList, frame.BackBuffer);
  commandList->EndRendering();
  commandList->End();
  device->ExecuteGraphicsCommandList(gecko::Move(commandList));
  device->Present(frame);
  return true;
}

bool PluginUpdate(const gecko::PluginFrame& frame) noexcept
{
  auto* windows = gecko::platform::GetWindows();
  if (!g_Running || windows == nullptr || !windows->IsWindowAlive(g_Window))
    return false;

  gecko::platform::PumpEvents();
  (void)gecko::DispatchEvents();
  if (!g_Headless && !RenderFrame(frame.FrameIndex))
    return false;

  GECKO_SLEEP_MS(1);
  return g_Running;
}

void PluginShutdown() noexcept
{
  g_ResizeSubscription.Reset();
  g_CloseSubscription.Reset();
  if (auto* device = gecko::graphics::GetGraphicsDevice(); device != nullptr)
    device->WaitIdle();
  gecko::debug_renderer::Shutdown();
  if (auto* device = gecko::graphics::GetGraphicsDevice(); device != nullptr && g_Swapchain.IsValid())
    device->DestroySwapchain(g_Swapchain);
  if (auto* windows = gecko::platform::GetWindows(); windows != nullptr && g_Window.IsValid())
    windows->DestroyWindow(g_Window);
  g_Swapchain = {};
  g_Window = {};
  g_ResizePending = false;
  g_Headless = false;
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
