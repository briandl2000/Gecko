#include "App.h"

#include "shaders.h"

#include <gecko/platform/platform_events.h>

namespace gecko::examples::graphics_example {

using namespace gecko::graphics;
using namespace gecko::platform;

namespace {

constexpr gecko::Label Main_Label = gecko::MakeLabel("app.graphics_example.main");

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

}  // namespace

App::App() noexcept
{
  gecko::GeckoConfig config {};
  config.AppName = "Gecko Graphics Example";
  config.GraphicsBackend = gecko::graphics::GraphicsBackend::Vulkan;
#if defined(_DEBUG)
  config.EnableGraphicsDebug = true;
#endif
  m_Initialized = gecko::Initialize(config) == gecko::InitializeResult::Success;
  if (!m_Initialized)
    return;

  m_Device = gecko::graphics::GetGraphicsDevice();
  m_GpuSampler = gecko::graphics::GetGpuSampler();
  if (!m_Device)
  {
    GECKO_ERROR(Main_Label, "GraphicsModule did not publish a device");
    return;
  }

  gecko::SetProfilerLevel(gecko::ProfLevel::Detailed);
  gecko::SetThreadProfilerName("main");
  GECKO_INFO(Main_Label, "Gecko {}", gecko::VersionFullString());

  if (!CreateWindows())
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

App::~App() noexcept
{
  m_CloseSubscription.Reset();
  m_ResizeSubscription.Reset();
  m_KeySubscription.Reset();

  m_TimestampPool = {};
  m_PlasmaPipeline = {};
  m_BlitPipeline = {};
  m_TrianglePipeline = {};
  m_BlitSampler = {};
  m_IndirectBuffer = {};
  m_VertexBuffer = {};
  m_PlasmaTextures[0] = {};
  m_PlasmaTextures[1] = {};
  m_OffscreenTarget = {};

  if (m_Device != nullptr)
  {
    for (auto& slot : m_Slots)
      if (slot.Swapchain.IsValid())
        m_Device->DestroySwapchain(slot.Swapchain);
  }
  if (m_Initialized)
  {
    for (auto& slot : m_Slots)
      if (slot.Handle.IsValid())
        gecko::platform::GetWindows()->DestroyWindow(slot.Handle);
    gecko::Shutdown();
  }
}

bool App::CreateWindows() noexcept
{
  static const char* WindowTitles[2] = {
      "Gecko Graphics - Window A",
      "Gecko Graphics - Window B",
  };
  for (gecko::u32 i = 0; i < 2; ++i)
  {
    WindowDesc wd;
    wd.Title = WindowTitles[i];
    wd.Size = {1280, 720};
    wd.Visible = true;
    wd.Resizable = false;
    wd.Mode = WindowMode::Windowed;
    m_Slots[i].Handle = gecko::platform::GetWindows()->CreateWindow(wd);
    if (!m_Slots[i].Handle.IsValid())
    {
      GECKO_ERROR(Main_Label, "Failed to create window {}", i);
      return false;
    }
  }
  GECKO_INFO(Main_Label, "Two windows created");
  return true;
}

bool App::CreateSwapchains() noexcept
{
  for (gecko::u32 i = 0; i < 2; ++i)
  {
    NativeWindowHandle native = gecko::platform::GetWindows()->GetNativeWindowHandle(m_Slots[i].Handle);
    Extent2D sz = gecko::platform::GetWindows()->GetClientSize(m_Slots[i].Handle);

    SwapchainDesc scDesc;
    scDesc.Width = sz.Width;
    scDesc.Height = sz.Height;
    scDesc.NumBackBuffers = 2;
    scDesc.Format = DataFormat::R8G8B8A8_UNORM;
    scDesc.VSync = false;

    m_Slots[i].Swapchain = m_Device->CreateSwapchain(native, scDesc);
    if (!m_Slots[i].Swapchain.IsValid())
      GECKO_WARN(Main_Label, "Swapchain {} not created (NullDevice?)", i);
  }
  return true;
}

bool App::CreateRenderResources() noexcept
{
  RenderTargetDesc rtDesc;
  rtDesc.Width = OffscreenW;
  rtDesc.Height = OffscreenH;
  rtDesc.Format = OffscreenFormat;
  rtDesc.Clear = ClearValue::RenderTarget(0.1F, 0.1F, 0.15F, 1.0F);
  rtDesc.DebugName = "Triangle Offscreen Target";
  m_OffscreenTarget = m_Device->CreateRenderTarget(rtDesc);
  if (m_OffscreenTarget.IsValid())
    GECKO_INFO(Main_Label, "Offscreen render target created ({}x{})", OffscreenW, OffscreenH);
  else
    GECKO_WARN(Main_Label, "Offscreen render target creation failed");

  TextureDesc plasmaDesc {};
  plasmaDesc.Width = OffscreenW;
  plasmaDesc.Height = OffscreenH;
  plasmaDesc.Format = DataFormat::R32G32B32A32_FLOAT;
  plasmaDesc.Type = TextureType::Tex2D;
  plasmaDesc.Memory = MemoryType::Dedicated;
  plasmaDesc.AllowUnorderedAccess = true;
  for (gecko::u32 i = 0; i < 2; ++i)
  {
    plasmaDesc.DebugName = (i == 0) ? "PlasmaStorageTexture[0]" : "PlasmaStorageTexture[1]";
    m_PlasmaTextures[i] = m_Device->CreateTexture(plasmaDesc);
  }
  if (m_PlasmaTextures[0].IsValid() && m_PlasmaTextures[1].IsValid())
    GECKO_INFO(Main_Label, "Plasma storage textures created (2x {}x{})", OffscreenW, OffscreenH);
  else
    GECKO_WARN(Main_Label, "Plasma storage texture creation failed");

  VertexBufferDesc vbDesc;
  vbDesc.NumVertices = 3;
  vbDesc.VertexSize = sizeof(Vertex);
  vbDesc.Memory = MemoryType::Dedicated;
  m_VertexBuffer = m_Device->CreateVertexBuffer(vbDesc);
  if (m_VertexBuffer.IsValid())
  {
    const auto* raw = reinterpret_cast<const gecko::byte*>(TriangleVertices);
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
    const gecko::u32 indirectArgs[4] = {3, 1, 0, 0};
    m_Device->UploadBufferData(m_IndirectBuffer,
                               {reinterpret_cast<const gecko::byte*>(indirectArgs), sizeof(indirectArgs)});
  }

  SamplerDesc blitSamplerDesc {};
  blitSamplerDesc.Filter = SamplerFilter::Linear;
  blitSamplerDesc.WrapMode = SamplerWrapMode::Clamp;
  blitSamplerDesc.DebugName = "BlitSampler";
  m_BlitSampler = m_Device->CreateSampler(blitSamplerDesc);
  return true;
}

bool App::CreatePipelines() noexcept
{
  namespace shader = gecko::examples::graphics_example::shaders;

  VertexLayout triLayout;
  triLayout.AddAttribute(DataFormat::R32G32B32_FLOAT, "a_Position");
  triLayout.AddAttribute(DataFormat::R32G32B32_FLOAT, "a_Color");

  GraphicsPipelineDesc triPDesc;
  triPDesc.VertexShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const gecko::byte*>(shader::TriangleVert), sizeof(shader::TriangleVert)},
  };
  triPDesc.PixelShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const gecko::byte*>(shader::TriangleFrag), sizeof(shader::TriangleFrag)},
  };
  triPDesc.Layout = triLayout;
  triPDesc.NumRenderTargets = 1;
  triPDesc.RenderTargetFormats[0] = OffscreenFormat;
  triPDesc.Culling = CullMode::None;
  triPDesc.PushConstantBytes = 16;
  triPDesc.DebugName = "TrianglePipeline";
  m_TrianglePipeline = m_Device->CreateGraphicsPipeline(triPDesc);

  GraphicsPipelineDesc blitPDesc;
  blitPDesc.VertexShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const gecko::byte*>(shader::FullscreenVert), sizeof(shader::FullscreenVert)},
  };
  blitPDesc.PixelShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const gecko::byte*>(shader::FullscreenFrag), sizeof(shader::FullscreenFrag)},
  };
  blitPDesc.Layout = {};
  blitPDesc.NumRenderTargets = 1;
  blitPDesc.RenderTargetFormats[0] =
      m_Slots[0].Swapchain.IsValid() ? m_Slots[0].Swapchain.Desc.Format : DataFormat::R8G8B8A8_UNORM;
  blitPDesc.Culling = CullMode::None;
  blitPDesc.PipelineResources[0] = PipelineResource::TextureBinding(1, ShaderType::Pixel);
  blitPDesc.PipelineResources[1] = PipelineResource::SamplerBinding(1, ShaderType::Pixel);
  blitPDesc.NumPipelineResources = 2;
  blitPDesc.PushConstantBytes = 16;
  blitPDesc.DebugName = "BlitPipeline";
  m_BlitPipeline = m_Device->CreateGraphicsPipeline(blitPDesc);

  ComputePipelineDesc plasmaPDesc;
  plasmaPDesc.ComputeShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const gecko::byte*>(shader::PlasmaComp), sizeof(shader::PlasmaComp)},
  };
  plasmaPDesc.PipelineResources[0] = PipelineResource::RWTextureBinding(1, ShaderType::Compute);
  plasmaPDesc.NumPipelineResources = 1;
  plasmaPDesc.PushConstantBytes = 16;
  plasmaPDesc.DebugName = "PlasmaComputePipeline";
  m_PlasmaPipeline = m_Device->CreateComputePipeline(plasmaPDesc);

  if (!m_TrianglePipeline.IsValid() || !m_BlitPipeline.IsValid())
  {
    GECKO_WARN(Main_Label, "One or more pipelines failed to build - rendering disabled");
  }
  else
  {
    GECKO_INFO(Main_Label, "Pipelines ready (triangle + blit + plasma)");
  }
  return true;
}

void App::CreateProfilingResources() noexcept
{
  QueryPoolDesc qpDesc {};
  qpDesc.Count = 4;
  qpDesc.DebugName = "FrameTimestamps";
  m_TimestampPool = m_Device->CreateTimestampQueryPool(qpDesc);

  if (m_GpuSampler)
    GECKO_INFO(Main_Label, "GPU profiler sampler available via service");
}

void App::SubscribeEvents() noexcept
{
  m_CloseSubscription = gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const gecko::EventMeta&, gecko::EventView) { static_cast<App*>(user)->m_Running = false; }, this);

  m_ResizeSubscription = gecko::SubscribeEvent(
      events::WindowResized,
      [](void* user, const gecko::EventMeta&, gecko::EventView view) {
        const auto* p = static_cast<const events::WindowResizedPayload*>(view.Data());
        auto* self = static_cast<App*>(user);
        for (gecko::u32 i = 0; i < 2; ++i)
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

  m_KeySubscription = gecko::SubscribeEvent(
      events::WindowKey,
      [](void* user, const gecko::EventMeta&, gecko::EventView view) {
        const auto* p = static_cast<const events::WindowKeyPayload*>(view.Data());
        if (!p->Down || p->Repeat)
          return;
        static_cast<App*>(user)->OnKey(p->Key);
      },
      this);
}

void App::OnKey(gecko::platform::KeyCode key) noexcept
{
  switch (key)
  {
  case KeyCode::Escape:
    m_Running = false;
    break;
  case KeyCode::F4:
    gecko::DumpProfilerStats(Main_Label);
    break;
  default:
    break;
  }
}

void App::HandlePendingResizes() noexcept
{
  for (auto& s : m_Slots)
  {
    if (!s.ResizeDirty)
      continue;
    if (s.ResizeW > 0 && s.ResizeH > 0)
    {
      s.Swapchain.Desc.Width = s.ResizeW;
      s.Swapchain.Desc.Height = s.ResizeH;
    }
    m_Device->ResizeSwapchain(s.Swapchain);
    s.ResizeDirty = false;
  }
}

void App::RecordComputePass(gecko::f32 time) noexcept
{
  const bool havePlasma = m_PlasmaPipeline.IsValid() && m_PlasmaTextures[0].IsValid() && m_PlasmaTextures[1].IsValid();
  if (!havePlasma)
    return;

  // Two compute cmd lists per frame to exercise the multi-cmd-list GPU
  // profiling path. List 0 dispatches the plasma; list 1 only performs
  // the SHADER_READ layout transition.
  gecko::Unique<ICommandList> computeCmd[2];
  for (gecko::u32 i = 0; i < 2; ++i)
  {
    computeCmd[i] = m_Device->CreateComputeCommandList();
    if (!computeCmd[i])
      continue;
    computeCmd[i]->Begin();
    if (m_GpuSampler)
      computeCmd[i]->AttachGpuSampler(m_GpuSampler, Main_Label);

    if (i == 0)
    {
      computeCmd[i]->BindPipeline(m_PlasmaPipeline);
      computeCmd[i]->BindRWTexture(0, m_PlasmaTextures[0]);
      const gecko::f32 pc[4] = {time, 0.0F, 0.0F, 0.0F};
      computeCmd[i]->SetConstants(0, {reinterpret_cast<const gecko::byte*>(pc), sizeof(pc)});
      const gecko::u32 gx = (OffscreenW + 15) / 16;
      const gecko::u32 gy = (OffscreenH + 15) / 16;
      {
        GECKO_GPU_SCOPE_NORMAL_NAMED(*computeCmd[i], Main_Label, "PlasmaPass");
        computeCmd[i]->Dispatch(gx, gy, 1);
      }
    }
    else
    {
      computeCmd[i]->TransitionTextureForRead(m_PlasmaTextures[0]);
    }
    computeCmd[i]->End();
  }
  for (gecko::u32 i = 0; i < 2; ++i)
    if (computeCmd[i])
      m_Device->ExecuteComputeCommandList(gecko::Move(computeCmd[i]));
}

void App::RecordTrianglePass(ICommandList& cmd, gecko::f32 time) noexcept
{
  ClearValue rtClear = ClearValue::RenderTarget(0.08F, 0.08F, 0.12F, 1.0F);
  const BeginRenderingInfo rendering {
      .Colors = {&m_OffscreenTarget, 1},
      .ClearColors = {&rtClear, 1},
  };
  cmd.BeginRendering(rendering);
  cmd.SetViewport(0.0F, 0.0F, static_cast<gecko::f32>(OffscreenW), static_cast<gecko::f32>(OffscreenH));
  cmd.SetScissor(0, 0, OffscreenW, OffscreenH);
  cmd.BindPipeline(m_TrianglePipeline);
  cmd.BindVertexBuffer(m_VertexBuffer);
  const gecko::f32 pc[4] = {time, 0.0F, 0.0F, 0.0F};
  cmd.SetConstants(0, {reinterpret_cast<const gecko::byte*>(pc), sizeof(pc)});

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

void App::RecordBlitPass(ICommandList& cmd, FrameContext (&frames)[2], gecko::f32 time) noexcept
{
  const Texture& triSampled = m_OffscreenTarget.BackingTexture;
  const bool havePlasma = m_PlasmaPipeline.IsValid() && m_PlasmaTextures[0].IsValid() && m_PlasmaTextures[1].IsValid();

  // Tint pulses 0.6..1.0 so push constants visibly affect the output.
  const gecko::f32 pulse = 0.8F + 0.2F * gecko::math::Sin(time * 2.0F);
  const gecko::f32 tint[4] = {pulse, pulse, pulse, 1.0F};

  const bool haveTimestamps = m_TimestampPool.IsValid();
  if (haveTimestamps)
    cmd.WriteTimestamp(m_TimestampPool, 2);
  {
    GECKO_GPU_SCOPE_NORMAL_NAMED(cmd, Main_Label, "BlitPass");
    for (gecko::u32 i = 0; i < 2; ++i)
    {
      if (!frames[i].Valid)
        continue;
      // Window 0: triangle offscreen RT. Window 1: plasma compute output.
      const Texture& src = (i == 1 && havePlasma) ? m_PlasmaTextures[0] : triSampled;
      ClearValue scClear = ClearValue::RenderTarget(0.0F, 0.0F, 0.0F, 1.0F);
      const BeginRenderingInfo rendering {
          .Colors = {&frames[i].BackBuffer, 1},
          .ClearColors = {&scClear, 1},
      };
      cmd.BeginRendering(rendering);
      cmd.SetViewport(0.0F, 0.0F, static_cast<gecko::f32>(frames[i].BackBuffer.Desc.Width),
                      static_cast<gecko::f32>(frames[i].BackBuffer.Desc.Height));
      cmd.SetScissor(0, 0, frames[i].BackBuffer.Desc.Width, frames[i].BackBuffer.Desc.Height);
      cmd.BindPipeline(m_BlitPipeline);
      cmd.BindTexture(0, src);
      cmd.BindSampler(1, m_BlitSampler);
      cmd.SetConstants(0, {reinterpret_cast<const gecko::byte*>(tint), sizeof(tint)});
      cmd.Draw(3);
      cmd.EndRendering();
    }
  }
  if (haveTimestamps)
    cmd.WriteTimestamp(m_TimestampPool, 3);
}

void App::RenderFrame() noexcept
{
  GECKO_SCOPE_ALWAYS_NAMED(Main_Label, "renderFrame");

  HandlePendingResizes();

  if (!m_TrianglePipeline.IsValid() || !m_BlitPipeline.IsValid() || !m_VertexBuffer.IsValid() ||
      !m_OffscreenTarget.IsValid())
    return;

  FrameContext frames[2] {};
  for (gecko::u32 i = 0; i < 2; ++i)
  {
    if (m_Slots[i].Swapchain.IsValid())
      frames[i] = m_Device->BeginFrame(m_Slots[i].Swapchain);
  }
  if (!frames[0].Valid && !frames[1].Valid)
    return;

  auto cmd = m_Device->CreateGraphicsCommandList();
  cmd->Begin();

  if (m_GpuSampler)
  {
    m_GpuSampler->BeginFrame(*cmd);
    cmd->AttachGpuSampler(m_GpuSampler, Main_Label);
  }
  if (m_TimestampPool.IsValid())
    cmd->ResetTimestamps(m_TimestampPool, 0, 4);

  const gecko::f32 time = gecko::time::NsToSecondsF(gecko::MonotonicTimeNs() - m_StartTimeNs);

  RecordComputePass(time);
  RecordTrianglePass(*cmd, time);
  RecordBlitPass(*cmd, frames, time);

  if (m_GpuSampler)
    m_GpuSampler->EndFrame(*cmd);

  cmd->End();
  m_Device->ExecuteGraphicsCommandList(gecko::Move(cmd));

  FrameContext toPresent[2] {};
  gecko::u32 presentCount = 0;
  gecko::u32 drawCalls = 1;
  for (gecko::u32 i = 0; i < 2; ++i)
    if (frames[i].Valid)
    {
      toPresent[presentCount++] = gecko::Move(frames[i]);
      ++drawCalls;
    }
  m_Device->Present(gecko::Span<const FrameContext> {toPresent, presentCount});

  GECKO_COUNTER(Main_Label, "DrawCalls", drawCalls);
  GECKO_COUNTER(Main_Label, "AllocLiveBytes", gecko::GetMemoryStats().LiveBytes);

  PrintHudIfDue(drawCalls);
  ++m_FrameIndex;
}

void App::PrintHudIfDue(gecko::u32 drawCalls) noexcept
{
  const gecko::u64 nowNs = gecko::ProfilerNowNs();
  if (nowNs - m_LastHudPrintNs <= 1'000'000'000ULL)
    return;
  m_LastHudPrintNs = nowNs;

  const auto frame = gecko::GetScopeStats("frame");
  const auto rend = gecko::GetScopeStats("renderFrame");
  const auto tri = gecko::GetScopeStats("TrianglePass", gecko::ProfSource::GPU);
  const auto blit = gecko::GetScopeStats("BlitPass", gecko::ProfSource::GPU);
  const gecko::f64 fps = (frame.AverageNs > 0) ? 1.0e9 / static_cast<gecko::f64>(frame.AverageNs) : 0.0;
  const gecko::u64 live = gecko::GetMemoryStats().LiveBytes;
  GECKO_INFO(Main_Label,
             "HUD frame={:.2}ms ({:.1} fps)  cpu_render={:.2}ms  "
             "gpu_tri={:.3}ms  gpu_blit={:.3}ms  draws={}  "
             "alloc_live={} KB  frames={}",
             frame.AverageNs / 1.0e6, fps, rend.AverageNs / 1.0e6, tri.AverageNs / 1.0e6, blit.AverageNs / 1.0e6,
             drawCalls, static_cast<unsigned long long>(live / 1024), static_cast<unsigned long long>(m_FrameIndex));
}

void App::Update() noexcept
{
  GECKO_SCOPE_ALWAYS_NAMED(Main_Label, "frame");
  (void)gecko::DispatchEvents();
  RenderFrame();
  GECKO_FRAME(Main_Label, "Frame");
}

int App::Run() noexcept
{
  if (!IsValid())
    return 1;

  GECKO_PUSH_LABEL(Main_Label);
  GECKO_SCOPE_ALWAYS_NAMED(Main_Label, "AppRun");

  GECKO_INFO(Main_Label, "Entering frame loop - Escape or close to quit");
  m_StartTimeNs = gecko::MonotonicTimeNs();

  gecko::platform::SetModalFrameCallback([](void* ud) { static_cast<App*>(ud)->Update(); }, this);

  while (m_Running)
  {
    gecko::platform::PumpEvents();
    Update();
  }

  gecko::platform::SetModalFrameCallback(nullptr, nullptr);
  GECKO_INFO(Main_Label, "Shutdown complete");
  return 0;
}

}  // namespace gecko::examples::graphics_example
