#include "App.h"

#include "shaders.h"

#include <cmath>
#include <cstdlib>
#include <gecko/core/labels.h>
#include <gecko/core/services.h>
#include <gecko/core/services/log.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>
#include <gecko/platform/platform_events.h>
#include <gecko/platform/windows_interface.h>
#include <utility>

namespace gecko::examples::graphics_example {

using namespace ::gecko::graphics;
using namespace ::gecko::platform;

namespace {

constexpr ::gecko::Label App_Label = ::gecko::MakeLabel("app.graphics_example");
constexpr ::gecko::Label Main_Label =
    ::gecko::MakeLabel("app.graphics_example.main");

struct Vertex
{
  float Position[3];
  float Color[3];
};

constexpr Vertex TriangleVertices[] = {
    {{0.0F, 0.5F, 0.0F}, {1.0F, 0.0F, 0.0F}},
    {{0.5F, -0.5F, 0.0F}, {0.0F, 1.0F, 0.0F}},
    {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
};

bool WantValidation()
{
  const char* v = ::std::getenv("GECKO_VK_VALIDATION");
  return v && v[0] != '\0' && v[0] != '0';
}

}  // namespace

App::AllocatorInstaller::AllocatorInstaller(::gecko::IAllocator* a) noexcept
    : Ok(::gecko::SetAllocator(a))
{}

App::AllocatorInstaller::~AllocatorInstaller()
{
  ::gecko::ResetAllocator();
}

::gecko::Label App::ExampleModule::RootLabel() const noexcept
{
  return App_Label;
}

bool App::ExampleModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::ExampleModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

App::App() : m_RuntimeModule(m_JobSystem, m_Profiler, m_Logger, m_EventBus)
{
  if (!m_AllocatorInstaller.Ok)
    return;

  m_Logger.SetThreadSafe(true);
  m_JobSystem.SetWorkerThreadCount(4);

  m_Engine = ::gecko::Engine::Create(
      {&m_RuntimeModule, &m_PlatformModule, &m_AppModule});
  if (!m_Engine)
    return;

  ConfigureProfiler();
  AttachSinks();
  ::gecko::SetThreadProfilerName("main");

  GECKO_INFO(Main_Label, ::gecko::VersionFullString());

  if (!CreateWindows())
    return;
  if (!CreateDevice())
    return;
  if (!CreateSwapchains())
    return;
  if (!CreateRenderResources())
    return;
  if (!CreatePipelines())
    return;
  CreateProfilingResources();
  SubscribeEvents();
}

App::~App()
{
  // Destruction is RAII-driven and *order-sensitive*.
  //
  // Every GPU resource handle (Buffer, Texture, RenderTarget, Pipeline,
  // QueryPool, Sampler, Swapchain) holds a `Shared<void>` whose deleter
  // captures a raw pointer to the owning device. They MUST destruct
  // before `m_Device`, otherwise their deleters dereference a dead
  // device and VMA fires an "allocations not freed" assert.
  //
  // Members are declared in the right order — m_Device first, every GPU
  // resource after it — so reverse-declaration destruction handles it.
  // We only do here what RAII cannot:
  //   1. Tear down swapchains explicitly so windows can be destroyed
  //      before the platform module shuts down.
  //   2. Destroy windows (the WindowHandle is just an ID, not RAII).
  //   3. Detach log sinks before the logger dies.
  if (m_Device)
  {
    for (auto& s : m_Slots)
      if (s.SC.IsValid())
        m_Device->DestroySwapchain(s.SC);
  }
  if (m_Engine)
  {
    for (auto& s : m_Slots)
      if (s.Handle.IsValid())
        ::gecko::platform::GetWindows()->DestroyWindow(s.Handle);
  }

  if (m_SinksAttached)
    DetachSinks();
}

void App::ConfigureProfiler()
{
#if defined(GECKO_DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
  ::gecko::GetProfiler()->SetMinLevel(::gecko::ProfLevel::Normal);
#else
  ::gecko::GetProfiler()->SetMinLevel(::gecko::ProfLevel::Always);
#endif
  if (auto* prof = ::gecko::GetProfiler())
  {
    prof->WatchScope("frame", 240);
    prof->WatchScope("renderFrame", 240);
    prof->WatchScope("TrianglePass", 240, ::gecko::ProfSource::GPU);
    prof->WatchScope("BlitPass", 240, ::gecko::ProfSource::GPU);
    prof->WatchScope("PlasmaCompute", 240);
    prof->SetStatsResetIntervalMs(500);
  }
}

void App::AttachSinks()
{
  if (auto* logger = ::gecko::GetLogger())
  {
    m_FileSink.RegisterWith(logger);
    m_ConsoleSink.RegisterWith(logger);
    logger->SetLevel(::gecko::LogLevel::Info);
  }
  m_SinksAttached = true;
}

void App::DetachSinks()
{
  m_ConsoleSink.Unregister();
  m_FileSink.Unregister();
  m_SinksAttached = false;
}

bool App::CreateWindows()
{
  static const char* WindowTitles[2] = {
      "Gecko Graphics - Window A",
      "Gecko Graphics - Window B",
  };
  for (::gecko::u32 i = 0; i < 2; ++i)
  {
    WindowDesc wd;
    wd.Title = WindowTitles[i];
    wd.Size = {1280, 720};
    wd.Visible = true;
    wd.Resizable = true;
    wd.Mode = WindowMode::Windowed;
    m_Slots[i].Handle = ::gecko::platform::GetWindows()->CreateWindow(wd);
    if (!m_Slots[i].Handle.IsValid())
    {
      GECKO_ERROR(Main_Label, "Failed to create window %u", i);
      return false;
    }
  }
  GECKO_INFO(Main_Label, "Two windows created");
  return true;
}

bool App::CreateDevice()
{
  m_Device = CreateGraphicsDevice(GraphicsDeviceDesc {
      .Backend = GraphicsBackend::Vulkan,
      .Debug = WantValidation(),
      .AppName = "graphics_example",
  });
  if (!m_Device)
  {
    GECKO_ERROR(Main_Label, "Failed to create graphics device");
    return false;
  }
  GECKO_INFO(Main_Label, "Graphics device created");
  return true;
}

bool App::CreateSwapchains()
{
  for (::gecko::u32 i = 0; i < 2; ++i)
  {
    NativeWindowHandle native =
        ::gecko::platform::GetWindows()->GetNativeWindowHandle(
            m_Slots[i].Handle);
    Extent2D sz =
        ::gecko::platform::GetWindows()->GetClientSize(m_Slots[i].Handle);

    SwapchainDesc scDesc;
    scDesc.Width = sz.Width;
    scDesc.Height = sz.Height;
    scDesc.NumBackBuffers = 2;
    scDesc.Format = DataFormat::R8G8B8A8_UNORM;
    scDesc.VSync = false;

    m_Slots[i].SC = m_Device->CreateSwapchain(native, scDesc);
    if (!m_Slots[i].SC.IsValid())
      GECKO_WARN(Main_Label, "Swapchain %u not created (NullDevice?)", i);
  }
  return true;
}

bool App::CreateRenderResources()
{
  RenderTargetDesc rtDesc;
  rtDesc.Width = OffscreenW;
  rtDesc.Height = OffscreenH;
  rtDesc.NumRenderTargets = 1;
  rtDesc.RenderTargetFormats[0] = OffscreenFmt;
  rtDesc.RenderTargetClearValues[0] =
      ClearValue::RenderTarget(0.1F, 0.1F, 0.15F, 1.0F);
  m_OffscreenRT = m_Device->CreateRenderTarget(rtDesc);
  if (m_OffscreenRT.IsValid())
    GECKO_INFO(Main_Label, "Offscreen render target created (%ux%u)",
               OffscreenW, OffscreenH);
  else
    GECKO_WARN(Main_Label, "Offscreen render target creation failed");

  TextureDesc plasmaDesc {};
  plasmaDesc.Width = OffscreenW;
  plasmaDesc.Height = OffscreenH;
  plasmaDesc.Format = DataFormat::R32G32B32A32_FLOAT;
  plasmaDesc.Type = TextureType::Tex2D;
  plasmaDesc.Memory = MemoryType::Dedicated;
  plasmaDesc.AllowUnorderedAccess = true;
  for (::gecko::u32 i = 0; i < 2; ++i)
  {
    plasmaDesc.DebugName =
        (i == 0) ? "PlasmaStorageTexture[0]" : "PlasmaStorageTexture[1]";
    m_PlasmaTex[i] = m_Device->CreateTexture(plasmaDesc);
  }
  if (m_PlasmaTex[0].IsValid() && m_PlasmaTex[1].IsValid())
    GECKO_INFO(Main_Label, "Plasma storage textures created (2x %ux%u)",
               OffscreenW, OffscreenH);
  else
    GECKO_WARN(Main_Label, "Plasma storage texture creation failed");

  VertexBufferDesc vbDesc;
  vbDesc.NumVertices = 3;
  vbDesc.VertexSize = sizeof(Vertex);
  vbDesc.Memory = MemoryType::Dedicated;
  m_VertexBuffer = m_Device->CreateVertexBuffer(vbDesc);
  if (m_VertexBuffer.IsValid())
  {
    const auto* raw = reinterpret_cast<const ::gecko::byte*>(TriangleVertices);
    m_Device->UploadBufferData(m_VertexBuffer, {raw, sizeof(TriangleVertices)});
  }

  // Indirect args buffer: VkDrawIndirectCommand layout
  // {vertexCount=3, instanceCount=1, firstVertex=0, firstInstance=0}.
  StructuredBufferDesc indirectDesc {};
  indirectDesc.NumElements = 1;
  indirectDesc.ElementSize = 16;
  indirectDesc.Memory = MemoryType::Dedicated;
  indirectDesc.DebugName = "TriangleIndirectArgs";
  m_IndirectBuffer = m_Device->CreateStructuredBuffer(indirectDesc);
  if (m_IndirectBuffer.IsValid())
  {
    const ::gecko::u32 indirectArgs[4] = {3, 1, 0, 0};
    m_Device->UploadBufferData(
        m_IndirectBuffer, {reinterpret_cast<const ::gecko::byte*>(indirectArgs),
                           sizeof(indirectArgs)});
  }

  SamplerDesc blitSamplerDesc {};
  blitSamplerDesc.Filter = SamplerFilter::Linear;
  blitSamplerDesc.WrapMode = SamplerWrapMode::Clamp;
  blitSamplerDesc.DebugName = "BlitSampler";
  m_BlitSampler = m_Device->CreateSampler(blitSamplerDesc);
  return true;
}

bool App::CreatePipelines()
{
  namespace shaders = ::gecko::graphics_example::shaders;

  VertexLayout triLayout;
  triLayout.AddAttribute(DataFormat::R32G32B32_FLOAT, "a_Position");
  triLayout.AddAttribute(DataFormat::R32G32B32_FLOAT, "a_Color");

  GraphicsPipelineDesc triPDesc;
  triPDesc.VertexShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::TriangleVert),
                sizeof(shaders::TriangleVert)},
  };
  triPDesc.PixelShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::TriangleFrag),
                sizeof(shaders::TriangleFrag)},
  };
  triPDesc.Layout = triLayout;
  triPDesc.NumRenderTargets = 1;
  triPDesc.RenderTargetFormats[0] = OffscreenFmt;
  triPDesc.Culling = CullMode::None;
  triPDesc.PushConstantBytes = 16;
  triPDesc.DebugName = "TrianglePipeline";
  m_TrianglePipeline = m_Device->CreateGraphicsPipeline(triPDesc);

  GraphicsPipelineDesc blitPDesc;
  blitPDesc.VertexShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::FullscreenVert),
                sizeof(shaders::FullscreenVert)},
  };
  blitPDesc.PixelShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::FullscreenFrag),
                sizeof(shaders::FullscreenFrag)},
  };
  blitPDesc.Layout = {};
  blitPDesc.NumRenderTargets = 1;
  blitPDesc.RenderTargetFormats[0] = m_Slots[0].SC.IsValid()
                                         ? m_Slots[0].SC.Desc.Format
                                         : DataFormat::R8G8B8A8_UNORM;
  blitPDesc.Culling = CullMode::None;
  blitPDesc.PipelineResources[0] =
      PipelineResource::TextureBinding(1, ShaderType::Pixel);
  blitPDesc.PipelineResources[1] =
      PipelineResource::SamplerBinding(1, ShaderType::Pixel);
  blitPDesc.NumPipelineResources = 2;
  blitPDesc.PushConstantBytes = 16;
  blitPDesc.DebugName = "BlitPipeline";
  m_BlitPipeline = m_Device->CreateGraphicsPipeline(blitPDesc);

  ComputePipelineDesc plasmaPDesc;
  plasmaPDesc.ComputeShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::PlasmaComp),
                sizeof(shaders::PlasmaComp)},
  };
  plasmaPDesc.PipelineResources[0] =
      PipelineResource::RWTextureBinding(1, ShaderType::Compute);
  plasmaPDesc.NumPipelineResources = 1;
  plasmaPDesc.PushConstantBytes = 16;
  plasmaPDesc.DebugName = "PlasmaComputePipeline";
  m_PlasmaPipeline = m_Device->CreateComputePipeline(plasmaPDesc);

  if (!m_TrianglePipeline.IsValid() || !m_BlitPipeline.IsValid())
  {
    GECKO_WARN(Main_Label,
               "One or more pipelines failed to build - rendering disabled");
  }
  else
  {
    GECKO_INFO(Main_Label, "Pipelines ready (triangle + blit + plasma)");
  }
  return true;
}

void App::CreateProfilingResources()
{
  QueryPoolDesc qpDesc {};
  qpDesc.Count = 4;
  qpDesc.DebugName = "FrameTimestamps";
  m_TimestampPool = m_Device->CreateTimestampQueryPool(qpDesc);

  GpuSamplerDesc samplerDesc {};
  samplerDesc.MaxZonesPerFrame = 64;
  samplerDesc.FramesInFlight = 3;
  samplerDesc.GpuThreadName = "GPU.Graphics";
  m_GpuSampler = m_Device->CreateGpuSampler(samplerDesc);
  if (m_GpuSampler)
    GECKO_INFO(Main_Label, "GPU profiler sampler created");
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
        for (::gecko::u32 i = 0; i < 2; ++i)
        {
          if (self->m_Slots[i].Handle == p->Window)
          {
            self->m_Slots[i].ResizeW = p->Width;
            self->m_Slots[i].ResizeH = p->Height;
            self->m_Slots[i].ResizeDirty = true;
          }
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
        static_cast<App*>(user)->OnKey(p->Key);
      },
      this);
}

void App::OnKey(::gecko::platform::KeyCode key)
{
  switch (key)
  {
  case KeyCode::Escape:
    m_Running = false;
    break;
  case KeyCode::F4:
    if (auto* prof = ::gecko::GetProfiler())
      prof->DumpStats(Main_Label);
    break;
  default:
    break;
  }
}

void App::HandlePendingResizes()
{
  for (auto& s : m_Slots)
  {
    if (!s.ResizeDirty)
      continue;
    if (s.ResizeW > 0 && s.ResizeH > 0)
    {
      s.SC.Desc.Width = s.ResizeW;
      s.SC.Desc.Height = s.ResizeH;
    }
    m_Device->ResizeSwapchain(s.SC);
    s.ResizeDirty = false;
  }
}

void App::RecordComputePass(::gecko::f32 time)
{
  const bool havePlasma = m_PlasmaPipeline.IsValid() &&
                          m_PlasmaTex[0].IsValid() && m_PlasmaTex[1].IsValid();
  if (!havePlasma)
    return;

  // Two compute cmd lists per frame to exercise the multi-cmd-list GPU
  // profiling path. List 0 dispatches the plasma; list 1 only performs
  // the SHADER_READ layout transition.
  ::gecko::Unique<ICommandList> computeCmd[2];
  for (::gecko::u32 i = 0; i < 2; ++i)
  {
    computeCmd[i] = m_Device->CreateComputeCommandList();
    if (!computeCmd[i])
      continue;
    computeCmd[i]->Begin();
    if (m_GpuSampler)
      computeCmd[i]->AttachGpuSampler(m_GpuSampler.get(), Main_Label);

    if (i == 0)
    {
      computeCmd[i]->BindPipeline(m_PlasmaPipeline);
      computeCmd[i]->BindRWTexture(0, m_PlasmaTex[0]);
      const ::gecko::f32 pc[4] = {time, 0.0F, 0.0F, 0.0F};
      computeCmd[i]->SetConstants(
          0, {reinterpret_cast<const ::gecko::byte*>(pc), sizeof(pc)});
      const ::gecko::u32 gx = (OffscreenW + 15) / 16;
      const ::gecko::u32 gy = (OffscreenH + 15) / 16;
      {
        GECKO_GPU_SCOPE_NORMAL_NAMED(*computeCmd[i], Main_Label, "PlasmaPass");
        computeCmd[i]->Dispatch(gx, gy, 1);
      }
    }
    else
    {
      computeCmd[i]->TransitionTextureForRead(m_PlasmaTex[0]);
    }
    computeCmd[i]->End();
  }
  for (::gecko::u32 i = 0; i < 2; ++i)
    if (computeCmd[i])
      m_Device->ExecuteComputeCommandList(::std::move(computeCmd[i]));
}

void App::RecordTrianglePass(ICommandList& cmd, ::gecko::f32 time)
{
  ClearValue rtClear = ClearValue::RenderTarget(0.08F, 0.08F, 0.12F, 1.0F);
  cmd.BeginRendering(m_OffscreenRT, &rtClear);
  cmd.SetViewport(0.0F, 0.0F, static_cast<::gecko::f32>(OffscreenW),
                  static_cast<::gecko::f32>(OffscreenH));
  cmd.SetScissor(0, 0, OffscreenW, OffscreenH);
  cmd.BindPipeline(m_TrianglePipeline);
  cmd.BindVertexBuffer(m_VertexBuffer);
  const ::gecko::f32 pc[4] = {time, 0.0F, 0.0F, 0.0F};
  cmd.SetConstants(0, {reinterpret_cast<const ::gecko::byte*>(pc), sizeof(pc)});

  const bool haveTimestamps = m_TimestampPool.IsValid();
  if (haveTimestamps)
    cmd.WriteTimestamp(m_TimestampPool, 0);
  {
    GECKO_GPU_SCOPE_NORMAL_NAMED(cmd, Main_Label, "TrianglePass");
    if (m_IndirectBuffer.IsValid())
      cmd.DrawIndirect(m_IndirectBuffer, 0, 1, 16);
    else
      cmd.Draw(3);
  }
  if (haveTimestamps)
    cmd.WriteTimestamp(m_TimestampPool, 1);
  cmd.EndRendering();
}

void App::RecordBlitPass(ICommandList& cmd, FrameContext (&frames)[2],
                         ::gecko::f32 time)
{
  const Texture& triSampled = m_OffscreenRT.RenderTextures[0];
  const bool havePlasma = m_PlasmaPipeline.IsValid() &&
                          m_PlasmaTex[0].IsValid() && m_PlasmaTex[1].IsValid();

  // Tint pulses 0.6..1.0 so push constants visibly affect the output.
  const ::gecko::f32 pulse = 0.8F + 0.2F * ::std::sin(time * 2.0F);
  const ::gecko::f32 tint[4] = {pulse, pulse, pulse, 1.0F};

  const bool haveTimestamps = m_TimestampPool.IsValid();
  if (haveTimestamps)
    cmd.WriteTimestamp(m_TimestampPool, 2);
  {
    GECKO_GPU_SCOPE_NORMAL_NAMED(cmd, Main_Label, "BlitPass");
    for (::gecko::u32 i = 0; i < 2; ++i)
    {
      if (!frames[i].Valid)
        continue;
      // Window 0: triangle offscreen RT. Window 1: plasma compute output.
      const Texture& src = (i == 1 && havePlasma) ? m_PlasmaTex[0] : triSampled;
      ClearValue scClear = ClearValue::RenderTarget(0.0F, 0.0F, 0.0F, 1.0F);
      cmd.BeginRendering(frames[i].BackBuffer, &scClear);
      cmd.SetViewport(
          0.0F, 0.0F,
          static_cast<::gecko::f32>(frames[i].BackBuffer.Desc.Width),
          static_cast<::gecko::f32>(frames[i].BackBuffer.Desc.Height));
      cmd.SetScissor(0, 0, frames[i].BackBuffer.Desc.Width,
                     frames[i].BackBuffer.Desc.Height);
      cmd.BindPipeline(m_BlitPipeline);
      cmd.BindTexture(0, src);
      cmd.BindSampler(1, m_BlitSampler);
      cmd.SetConstants(
          0, {reinterpret_cast<const ::gecko::byte*>(tint), sizeof(tint)});
      cmd.Draw(3);
      cmd.EndRendering();
    }
  }
  if (haveTimestamps)
    cmd.WriteTimestamp(m_TimestampPool, 3);
}

void App::RenderFrame()
{
  GECKO_SCOPE_ALWAYS_NAMED(Main_Label, "renderFrame");

  HandlePendingResizes();

  if (!m_TrianglePipeline.IsValid() || !m_BlitPipeline.IsValid() ||
      !m_VertexBuffer.IsValid() || !m_OffscreenRT.IsValid())
    return;

  FrameContext frames[2] {};
  for (::gecko::u32 i = 0; i < 2; ++i)
  {
    if (m_Slots[i].SC.IsValid())
      frames[i] = m_Device->BeginFrame(m_Slots[i].SC);
  }
  if (!frames[0].Valid && !frames[1].Valid)
    return;

  auto cmd = m_Device->CreateGraphicsCommandList();
  cmd->Begin();

  if (m_GpuSampler)
  {
    m_GpuSampler->BeginFrame(*cmd);
    cmd->AttachGpuSampler(m_GpuSampler.get(), Main_Label);
  }
  if (m_TimestampPool.IsValid())
    cmd->ResetTimestamps(m_TimestampPool, 0, 4);

  const ::gecko::f32 time =
      ::std::chrono::duration<::gecko::f32>(::std::chrono::steady_clock::now() -
                                            m_StartTime)
          .count();

  RecordComputePass(time);
  RecordTrianglePass(*cmd, time);
  RecordBlitPass(*cmd, frames, time);

  if (m_GpuSampler)
    m_GpuSampler->EndFrame(*cmd);

  cmd->End();
  m_Device->ExecuteGraphicsCommandList(::std::move(cmd));

  FrameContext toPresent[2] {};
  ::gecko::u32 presentCount = 0;
  for (::gecko::u32 i = 0; i < 2; ++i)
    if (frames[i].Valid)
      toPresent[presentCount++] = ::std::move(frames[i]);
  m_Device->Present(::std::span<const FrameContext> {toPresent, presentCount});

  ::gecko::u32 drawCalls = 1u;
  for (const auto& f : frames)
    if (f.Valid)
      ++drawCalls;
  GECKO_COUNTER(Main_Label, "DrawCalls", drawCalls);
  GECKO_COUNTER(Main_Label, "AllocLiveBytes", m_Allocator.TotalLiveBytes());

  PrintHudIfDue(drawCalls);
  ++m_FrameIndex;
}

void App::PrintHudIfDue(::gecko::u32 drawCalls)
{
  auto* prof = ::gecko::GetProfiler();
  if (!prof)
    return;
  const ::gecko::u64 nowNs = prof->NowNs();
  if (nowNs - m_LastHudPrintNs <= 1'000'000'000ULL)
    return;
  m_LastHudPrintNs = nowNs;

  const auto frame = prof->GetStats("frame");
  const auto rend = prof->GetStats("renderFrame");
  const auto tri = prof->GetStats("TrianglePass", ::gecko::ProfSource::GPU);
  const auto blit = prof->GetStats("BlitPass", ::gecko::ProfSource::GPU);
  const ::gecko::f64 fps =
      (frame.AvgNs > 0) ? 1.0e9 / static_cast<::gecko::f64>(frame.AvgNs) : 0.0;
  const ::gecko::u64 live = m_Allocator.TotalLiveBytes();
  GECKO_INFO(Main_Label,
             "HUD frame=%.2fms (%.1f fps)  cpu_render=%.2fms  "
             "gpu_tri=%.3fms  gpu_blit=%.3fms  draws=%u  "
             "alloc_live=%llu KB  frames=%llu",
             frame.AvgNs / 1.0e6, fps, rend.AvgNs / 1.0e6, tri.AvgNs / 1.0e6,
             blit.AvgNs / 1.0e6, drawCalls,
             static_cast<unsigned long long>(live / 1024),
             static_cast<unsigned long long>(m_FrameIndex));
}

void App::Update()
{
  GECKO_SCOPE_ALWAYS_NAMED(Main_Label, "frame");
  (void)::gecko::DispatchEvents();
  RenderFrame();
  GECKO_FRAME(Main_Label, "Frame");
}

int App::Run()
{
  if (!IsValid())
    return 1;

  GECKO_PUSH_LABEL(Main_Label);
  GECKO_SCOPE_ALWAYS_NAMED(Main_Label, "AppRun");

  GECKO_INFO(Main_Label, "Entering frame loop - Escape or close to quit");
  m_StartTime = ::std::chrono::steady_clock::now();

  ::gecko::platform::SetModalFrameCallback(
      [](void* ud) { static_cast<App*>(ud)->Update(); }, this);

  while (m_Running)
  {
    ::gecko::platform::PumpEvents();
    Update();
  }

  ::gecko::platform::SetModalFrameCallback(nullptr, nullptr);
  GECKO_INFO(Main_Label, "Shutdown complete");
  return 0;
}

}  // namespace gecko::examples::graphics_example
