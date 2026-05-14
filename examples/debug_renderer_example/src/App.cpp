#include "App.h"

#include "gecko/math/vector.h"

#include <gecko/core/labels.h>
#include <gecko/core/services.h>
#include <gecko/core/services/log.h>
#include <gecko/core/utility/random.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>
#include <gecko/debug_renderer/debug_renderer_context.h>
#include <gecko/math/math.h>
#include <gecko/platform/platform_events.h>
#include <gecko/platform/windows_interface.h>
#include <utility>

namespace app::debug_renderer_example {

using namespace ::gecko::graphics;
using namespace ::gecko::platform;

namespace {

constexpr ::gecko::Label App_Label = ::gecko::MakeLabel("app.debug_renderer_example");
constexpr ::gecko::Label Main_Label = ::gecko::MakeLabel("app.debug_renderer_example.main");

}  // namespace

::gecko::Label App::AppModule::RootLabel() const noexcept
{
  return App_Label;
}

bool App::AppModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::AppModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

::gecko::graphics::GraphicsConfig App::MakeGraphicsConfig() noexcept
{
  ::gecko::graphics::GraphicsConfig cfg {};
  cfg.Backend = ::gecko::graphics::GraphicsBackend::Vulkan;
  cfg.Debug = false;
  cfg.AppName = "debug_renderer_example";
  return cfg;
}

App::App() : m_GraphicsModule(MakeGraphicsConfig())
{
  if (!m_AllocScope)
    return;

  m_Engine = ::gecko::Engine::Create(
      {&m_RuntimeModule, &m_PlatformModule, &m_GraphicsModule, &m_DebugRendererModule, &m_AppModule});
  if (!m_Engine)
    return;

  m_Device = ::gecko::graphics::GetGraphicsDevice();
  if (!m_Device)
  {
    GECKO_ERROR(Main_Label, "GraphicsModule did not publish a device");
    return;
  }

  m_LogSinks.emplace();
  ::gecko::SetThreadProfilerName("main");
  GECKO_INFO(Main_Label, "Gecko %s", ::gecko::VersionFullString());

  if (!CreateMainWindow())
    return;
  if (!CreateSwapchain())
    return;
  SubscribeEvents();

  m_DebugRendererContext = ::gecko::CreateShared<gecko::debug_renderer::DebugRendererContext>();
  if (!m_DebugRendererContext || !m_DebugRendererContext->IsValid())
  {
    GECKO_ERROR(Main_Label, "Failed to create DebugRendererContext");
    m_DebugRendererContext.reset();
    return;
  }
}

App::~App()
{
  // GPU resources (Swapchain, Buffer, Pipeline) hold a Shared<void>
  // whose deleter captures a raw pointer to the device. They MUST
  // destruct before the device, which lives in m_GraphicsModule and
  // is torn down by m_Engine.reset(). Members are declared so these
  // resources sit AFTER m_Engine in the class, so reverse-order
  // destruction handles them automatically.
  //
  // We only do here what RAII cannot: destroy the swapchain explicitly
  // so the window can be destroyed before the platform module shuts
  // down, and destroy the window (WindowHandle is just an ID).

  m_DebugRendererContext.reset();

  if (m_Device && m_Swapchain.IsValid())
    m_Device->DestroySwapchain(m_Swapchain);

  if (m_Engine && m_Window.IsValid())
    GetWindows()->DestroyWindow(m_Window);
}

bool App::CreateMainWindow()
{
  WindowDesc wd;
  wd.Title = "Gecko Debug Renderer Example";
  wd.Size = {1280, 720};
  wd.Visible = true;
  wd.Resizable = false;
  wd.Mode = WindowMode::Windowed;
  m_Window = GetWindows()->CreateWindow(wd);
  if (!m_Window.IsValid())
  {
    GECKO_ERROR(Main_Label, "Failed to create window");
    return false;
  }
  return true;
}

bool App::CreateSwapchain()
{
  NativeWindowHandle native = GetWindows()->GetNativeWindowHandle(m_Window);
  Extent2D sz = GetWindows()->GetClientSize(m_Window);

  SwapchainDesc scDesc;
  scDesc.Width = sz.Width;
  scDesc.Height = sz.Height;
  scDesc.NumBackBuffers = 2;
  scDesc.Format = DataFormat::R8G8B8A8_UNORM;
  scDesc.VSync = true;

  m_Swapchain = m_Device->CreateSwapchain(native, scDesc);
  if (!m_Swapchain.IsValid())
  {
    GECKO_ERROR(Main_Label, "Failed to create swapchain");
    return false;
  }
  return true;
}

void App::SubscribeEvents()
{
  m_CloseSub = ::gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView) { static_cast<App*>(user)->m_Running = false; },
      this);

  m_ResizeSub = ::gecko::SubscribeEvent(
      events::WindowResized,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView view) {
        const auto* p = static_cast<const events::WindowResizedPayload*>(view.Data());
        auto* self = static_cast<App*>(user);
        if (self->m_Window == p->Window)
        {
          self->m_ResizeW = p->Width;
          self->m_ResizeH = p->Height;
          self->m_ResizeDirty = true;
        }
      },
      this);

  m_KeySub = ::gecko::SubscribeEvent(
      events::WindowKey,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView view) {
        const auto* p = static_cast<const events::WindowKeyPayload*>(view.Data());
        if (!p->Down || p->Repeat)
          return;
        if (p->Key == KeyCode::Escape)
          static_cast<App*>(user)->m_Running = false;
      },
      this);
}

void App::HandlePendingResize()
{
  if (!m_ResizeDirty)
    return;
  if (m_ResizeW > 0 && m_ResizeH > 0)
  {
    m_Swapchain.Desc.Width = m_ResizeW;
    m_Swapchain.Desc.Height = m_ResizeH;
  }
  m_Device->ResizeSwapchain(m_Swapchain);
  m_ResizeDirty = false;
}

void App::RenderFrame()
{
  HandlePendingResize();

  FrameContext frame = m_Device->BeginFrame(m_Swapchain);
  if (!frame.Valid)
    return;

  auto DrawCircle = [this](::gecko::math::float2 center, ::gecko::f32 radius, ::gecko::math::float3 color,
                           ::gecko::f32 thickness) {
    constexpr ::gecko::u32 kSegments = 32;
    for (::gecko::u32 i = 0; i < kSegments; ++i)
    {
      const ::gecko::f32 a1 = (static_cast<::gecko::f32>(i) / kSegments) * ::gecko::math::TwoPi;
      const ::gecko::f32 a2 = (static_cast<::gecko::f32>(i + 1) / kSegments) * ::gecko::math::TwoPi;
      const ::gecko::math::float2 p1 {center.X + radius * ::std::cos(a1), center.Y + radius * ::std::sin(a1)};
      const ::gecko::math::float2 p2 {center.X + radius * ::std::cos(a2), center.Y + radius * ::std::sin(a2)};
      m_DebugRendererContext->DrawLine(p1, p2, color, thickness);
    }
  };

  auto cmd = m_Device->CreateGraphicsCommandList();
  cmd->Begin();
  m_DebugRendererContext->NewFrame();

  ::gecko::graphics::ClearValue clear = ::gecko::graphics::ClearValue::RenderTarget(0.05F, 0.05F, 0.08F, 1.0F);
  cmd->BeginRendering(frame.BackBuffer, &clear);
  m_DebugRendererContext->SetFrame(frame.BackBuffer);

  const auto fbW = static_cast<::gecko::f32>(frame.BackBuffer.Desc.Width);
  const auto fbH = static_cast<::gecko::f32>(frame.BackBuffer.Desc.Height);

  gecko::math::Float2 center = {fbW / 2.0f, fbH / 2.0f};
  gecko::f32 radius = (fbW > fbH ? fbH : fbW) * 0.4f;
  DrawCircle(center, radius, {0.2F, 0.3F, 9.0F}, 2.0F);

  const auto mousePos = GetInput()->GetMousePosition(m_Window);
  const ::gecko::math::float2 mousePosF {static_cast<::gecko::f32>(mousePos.X), static_cast<::gecko::f32>(mousePos.Y)};
  DrawCircle(mousePosF, 10.0F, {0.0F, 1.0F, 0.0F}, 4.0F);
  m_DebugRendererContext->DrawLine(center, mousePosF, {0.3f, 0.9f, 0.4f}, 2.f);

  m_DebugRendererContext->DrawText("Hello World!", {100.0f, 100.0f}, {.9f, .9f, .9f}, 50.0f);

  m_DebugRendererContext->Submit(cmd.get());
  cmd->EndRendering();

  m_DebugRendererContext->EndFrame();
  cmd->End();
  m_Device->ExecuteGraphicsCommandList(::std::move(cmd));

  FrameContext toPresent[1] = {::std::move(frame)};
  m_Device->Present(::std::span<const FrameContext> {toPresent, 1});
}

void App::Update()
{
  (void)::gecko::DispatchEvents();

  RenderFrame();
}

int App::Run()
{
  if (!IsValid())
    return 1;

  GECKO_INFO(Main_Label, "Entering frame loop - Escape or close to quit");

  SetModalFrameCallback([](void* ud) { static_cast<App*>(ud)->Update(); }, this);

  while (m_Running)
  {
    PumpEvents();
    Update();
  }

  SetModalFrameCallback(nullptr, nullptr);
  GECKO_INFO(Main_Label, "Shutdown complete");
  return 0;
}

}  // namespace app::debug_renderer_example
