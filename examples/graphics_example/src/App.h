#pragma once

#include "gecko/gecko.h"

namespace gecko::examples::graphics_example {

class App
{
public:
  App() noexcept;
  ~App() noexcept;

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Initialized && m_Device != nullptr && m_Slots[0].Handle.IsValid() && m_Slots[1].Handle.IsValid();
  }

  int Run() noexcept;

private:
  static constexpr u32 OffscreenW = 1280;
  static constexpr u32 OffscreenH = 720;
  static constexpr graphics::DataFormat OffscreenFormat = graphics::DataFormat::R8G8B8A8_UNORM;

  struct WindowSlot
  {
    platform::WindowHandle Handle {};
    graphics::Swapchain Swapchain {};
    bool ResizeDirty {false};
    u32 ResizeW {0};
    u32 ResizeH {0};
  };

  bool CreateWindows() noexcept;
  bool CreateSwapchains() noexcept;
  bool CreateRenderResources() noexcept;
  bool CreatePipelines() noexcept;
  void CreateProfilingResources() noexcept;
  void SubscribeEvents() noexcept;
  void HandlePendingResizes() noexcept;
  void RecordComputePass(f32 time) noexcept;
  void RecordTrianglePass(graphics::ICommandList& commands, f32 time) noexcept;
  void RecordBlitPass(graphics::ICommandList& commands, graphics::FrameContext (&frames)[2], f32 time) noexcept;
  void RenderFrame() noexcept;
  void Update() noexcept;
  void OnKey(platform::KeyCode key) noexcept;
  void PrintHudIfDue(u32 drawCalls) noexcept;

  bool m_Initialized {false};
  graphics::GraphicsDevice* m_Device {nullptr};
  graphics::IGpuSampler* m_GpuSampler {nullptr};
  WindowSlot m_Slots[2] {};

  graphics::RenderTarget m_OffscreenTarget {};
  graphics::Texture m_PlasmaTextures[2] {};
  graphics::Buffer m_VertexBuffer {};
  graphics::Buffer m_IndirectBuffer {};
  graphics::Sampler m_BlitSampler {};
  graphics::QueryPool m_TimestampPool {};
  graphics::GraphicsPipeline m_TrianglePipeline {};
  graphics::GraphicsPipeline m_BlitPipeline {};
  graphics::ComputePipeline m_PlasmaPipeline {};

  EventSubscription m_CloseSubscription {};
  EventSubscription m_ResizeSubscription {};
  EventSubscription m_KeySubscription {};
  bool m_Running {true};
  u64 m_StartTimeNs {0};
  u64 m_FrameIndex {0};
  u64 m_LastHudPrintNs {0};
};

}  // namespace gecko::examples::graphics_example
