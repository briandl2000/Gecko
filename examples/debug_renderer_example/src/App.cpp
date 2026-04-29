#include "App.h"

#include "shaders.h"

#include <gecko/core/labels.h>
#include <gecko/core/services/log.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>
#include <gecko/math/math.h>
#include <gecko/platform/platform_events.h>
#include <gecko/platform/windows_interface.h>
#include <utility>

namespace app::debug_renderer_example {

using namespace ::gecko::graphics;
using namespace ::gecko::platform;

namespace labels {
inline constexpr ::gecko::Label App =
    ::gecko::MakeLabel("app.debug_renderer_example");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.debug_renderer_example.main");
}  // namespace labels

namespace {

/// One screenspace debug line. Mirrors the HLSL `DebugLine` struct.
/// Layout (32 B): float2 A | float2 B | float3 Color | float Thickness.
struct DebugLine
{
  gecko::math::float2 A;      ///< Start, pixels, top-left origin.
  gecko::math::float2 B;      ///< End,   pixels.
  gecko::math::float3 Color;  ///< RGB, linear, [0,1].
  gecko::f32 Thickness;       ///< Width in pixels.
};

static constexpr DebugLine DebugLines[] = {
    {{100.0f, 100.0f}, {500.0f, 300.0f}, {1.0f, 1.0f, 0.0f}, 1.0f},
    {{200.0f, 500.0f}, {800.0f, 150.0f}, {0.0f, 1.0f, 1.0f}, 1.0f},
    {{640.0f, 50.0f}, {640.0f, 670.0f}, {1.0f, 0.2f, 0.2f}, 1.0f},
};

struct DebugLinePushConstants
{
  gecko::math::float2 ViewportPx;
  gecko::math::float2 _Pad;
};

}  // namespace

App::AllocatorInstaller::AllocatorInstaller(::gecko::IAllocator* a) noexcept
    : Ok(::gecko::SetAllocator(a))
{}

App::AllocatorInstaller::~AllocatorInstaller()
{
  ::gecko::ResetAllocator();
}

::gecko::Label App::AppModule::RootLabel() const noexcept
{
  return labels::App;
}

bool App::AppModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::AppModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

App::App() : m_RuntimeModule(m_JobSystem, m_Profiler, m_Logger, m_EventBus)
{
  if (!m_AllocatorInstaller.Ok)
    return;

  m_JobSystem.SetWorkerThreadCount(4);

  m_Engine = ::gecko::Engine::Create(
      {&m_RuntimeModule, &m_PlatformModule, &m_AppModule});
  if (!m_Engine)
    return;

  AttachSinks();
  ::gecko::SetThreadProfilerName("main");
  GECKO_INFO(labels::Main, "Gecko %s", ::gecko::VersionFullString());

  if (!CreateMainWindow())
    return;
  if (!CreateDevice())
    return;
  if (!CreateSwapchain())
    return;
  if (!CreateRenderResources())
    return;
  if (!CreatePipeline())
    return;
  SubscribeEvents();
}

App::~App()
{
  if (m_Device && m_Swapchain.IsValid())
    m_Device->DestroySwapchain(m_Swapchain);

  if (m_Engine && m_Window.IsValid())
    GetWindows()->DestroyWindow(m_Window);

  if (m_SinksAttached)
    DetachSinks();

  m_Engine.reset();
}

void App::AttachSinks()
{
  if (auto* logger = ::gecko::GetLogger())
  {
    m_ConsoleSink.RegisterWith(logger);
    logger->SetLevel(::gecko::LogLevel::Info);
  }
  m_SinksAttached = true;
}

void App::DetachSinks()
{
  m_ConsoleSink.Unregister();
  m_SinksAttached = false;
}

bool App::CreateMainWindow()
{
  WindowDesc wd;
  wd.Title = "Gecko Debug Renderer Example";
  wd.Size = {1280, 720};
  wd.Visible = true;
  wd.Resizable = true;
  wd.Mode = WindowMode::Windowed;
  m_Window = GetWindows()->CreateWindow(wd);
  if (!m_Window.IsValid())
  {
    GECKO_ERROR(labels::Main, "Failed to create window");
    return false;
  }
  return true;
}

bool App::CreateDevice()
{
  m_Device = CreateGraphicsDevice(GraphicsDeviceDesc {
      .Backend = GraphicsBackend::Vulkan,
      .Debug = false,
      .AppName = "debug_renderer_example",
  });
  if (!m_Device)
  {
    GECKO_ERROR(labels::Main, "Failed to create graphics device");
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
    GECKO_ERROR(labels::Main, "Failed to create swapchain");
    return false;
  }
  return true;
}

bool App::CreateRenderResources()
{
  StructuredBufferDesc vbDesc;
  vbDesc.ElementSize = sizeof(DebugLine);
  vbDesc.NumElements = static_cast<gecko::u32>(std::size(DebugLines));
  vbDesc.Memory = MemoryType::Dedicated;
  m_VertexBuffer = m_Device->CreateStructuredBuffer(vbDesc);
  if (!m_VertexBuffer.IsValid())
  {
    GECKO_ERROR(labels::Main, "Failed to create line buffer");
    return false;
  }
  const auto* raw = reinterpret_cast<const ::gecko::byte*>(DebugLines);
  m_Device->UploadBufferData(m_VertexBuffer, {raw, sizeof(DebugLines)});
  return true;
}

bool App::CreatePipeline()
{
  namespace shaders = ::app::debug_renderer_example::shaders;

  GraphicsPipelineDesc pDesc;
  pDesc.VertexShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineVert),
                sizeof(shaders::DebugLineVert)},
  };
  pDesc.PixelShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineFrag),
                sizeof(shaders::DebugLineFrag)},
  };
  pDesc.PipelineResources[0] =
      PipelineResource::StructuredBufferBinding(1, ShaderType::Vertex);
  pDesc.NumPipelineResources = 1;
  pDesc.PushConstantBytes = sizeof(DebugLinePushConstants);
  pDesc.NumRenderTargets = 1;
  pDesc.RenderTargetFormats[0] = m_Swapchain.Desc.Format;
  pDesc.Culling = CullMode::None;
  pDesc.DebugName = "DebugLinePipeline";
  m_DebugLinePipeline = m_Device->CreateGraphicsPipeline(pDesc);

  if (!m_DebugLinePipeline.IsValid())
  {
    GECKO_ERROR(labels::Main, "Failed to create debug line pipeline");
    return false;
  }
  return true;
}

void App::SubscribeEvents()
{
  m_CloseSub = ::gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView) {
        static_cast<App*>(user)->m_Running = false;
      },
      this);

  m_ResizeSub = ::gecko::SubscribeEvent(
      events::WindowResized,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView view) {
        const auto* p =
            static_cast<const events::WindowResizedPayload*>(view.Data());
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
        const auto* p =
            static_cast<const events::WindowKeyPayload*>(view.Data());
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

  if (!m_DebugLinePipeline.IsValid() || !m_VertexBuffer.IsValid() ||
      !m_Swapchain.IsValid())
    return;

  FrameContext frame = m_Device->BeginFrame(m_Swapchain);
  if (!frame.Valid)
    return;

  auto cmd = m_Device->CreateGraphicsCommandList();
  cmd->Begin();

  ClearValue clear = ClearValue::RenderTarget(0.05F, 0.05F, 0.08F, 1.0F);
  cmd->BeginRendering(frame.BackBuffer, &clear);
  cmd->SetViewport(0.0F, 0.0F,
                   static_cast<::gecko::f32>(frame.BackBuffer.Desc.Width),
                   static_cast<::gecko::f32>(frame.BackBuffer.Desc.Height));
  cmd->SetScissor(0, 0, frame.BackBuffer.Desc.Width,
                  frame.BackBuffer.Desc.Height);
  cmd->BindPipeline(m_DebugLinePipeline);
  cmd->BindStructuredBuffer(0, m_VertexBuffer);

  DebugLinePushConstants pc {
      .ViewportPx = {static_cast<::gecko::f32>(frame.BackBuffer.Desc.Width),
                     static_cast<::gecko::f32>(frame.BackBuffer.Desc.Height)},
      ._Pad = {0.0f, 0.0f},
  };
  cmd->SetConstants(0,
                    gecko::Span<const gecko::byte>(
                        reinterpret_cast<const gecko::byte*>(&pc), sizeof(pc)));

  const ::gecko::u32 numLines =
      static_cast<::gecko::u32>(std::size(DebugLines));
  cmd->Draw(numLines * 6u);
  cmd->EndRendering();

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

  GECKO_INFO(labels::Main, "Entering frame loop - Escape or close to quit");

  SetModalFrameCallback([](void* ud) { static_cast<App*>(ud)->Update(); },
                        this);

  while (m_Running)
  {
    PumpEvents();
    Update();
  }

  SetModalFrameCallback(nullptr, nullptr);
  GECKO_INFO(labels::Main, "Shutdown complete");
  return 0;
}

}  // namespace app::debug_renderer_example
