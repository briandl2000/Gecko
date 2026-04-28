#pragma once

#include <chrono>
#include <gecko/core/engine.h>
#include <gecko/core/ptr.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/events.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/graphics/gpu_profiler.h>
#include <gecko/graphics/graphics_device.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/file_log_sink.h>
#include <gecko/runtime/immediate_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace gecko::examples::graphics_example {

/// Two-window Vulkan demo with a triangle, a fullscreen blit and a
/// compute-driven plasma. Boots the same service stack as
/// `platform_example` plus `gecko::graphics`.
///
/// Owns every GPU resource as a member so destruction order matches
/// the device lifetime.
class App
{
public:
  App();
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value() && m_Device != nullptr &&
           m_Slots[0].Handle.IsValid() && m_Slots[1].Handle.IsValid();
  }

  /// Enters the main loop. Returns the process exit code.
  int Run();

private:
  static constexpr ::gecko::u32 OffscreenW = 1280;
  static constexpr ::gecko::u32 OffscreenH = 720;
  static constexpr ::gecko::graphics::DataFormat OffscreenFmt =
      ::gecko::graphics::DataFormat::R8G8B8A8_UNORM;

  struct AllocatorInstaller
  {
    explicit AllocatorInstaller(::gecko::IAllocator* a) noexcept;
    ~AllocatorInstaller();
    AllocatorInstaller(const AllocatorInstaller&) = delete;
    AllocatorInstaller& operator=(const AllocatorInstaller&) = delete;
    bool Ok = false;
  };

  class ExampleModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  /// One swapchain + queued resize for one of the two demo windows.
  struct WindowSlot
  {
    ::gecko::platform::WindowHandle Handle {};
    ::gecko::graphics::Swapchain SC {};
    bool ResizeDirty = false;
    ::gecko::u32 ResizeW = 0;
    ::gecko::u32 ResizeH = 0;
  };

  void AttachSinks();
  void DetachSinks();

  bool CreateWindows();
  bool CreateDevice();
  bool CreateSwapchains();
  bool CreateRenderResources();
  bool CreatePipelines();
  void CreateProfilingResources();
  void ConfigureProfiler();
  void SubscribeEvents();

  void HandlePendingResizes();
  void RecordComputePass(::gecko::f32 time);
  void RecordTrianglePass(::gecko::graphics::ICommandList& cmd,
                          ::gecko::f32 time);
  void RecordBlitPass(::gecko::graphics::ICommandList& cmd,
                      ::gecko::graphics::FrameContext (&frames)[2],
                      ::gecko::f32 time);
  void RenderFrame();
  void Update();
  void OnKey(::gecko::platform::KeyCode key);
  void PrintHudIfDue(::gecko::u32 drawCalls);

  // Services / modules / sinks.
  ::gecko::runtime::TrackingAllocator m_Allocator;
  AllocatorInstaller m_AllocatorInstaller {&m_Allocator};

  ::gecko::runtime::RingProfiler m_Profiler {1 << 20};
  ::gecko::runtime::ImmediateLogger m_Logger;
  ::gecko::runtime::EventBus m_EventBus;
  ::gecko::runtime::ThreadPoolJobSystem m_JobSystem;

  ::gecko::runtime::CoreServicesModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  ExampleModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;

  ::gecko::runtime::ConsoleLogSink m_ConsoleSink;
  ::gecko::runtime::FileLogSink m_FileSink {"log.txt"};
  bool m_SinksAttached = false;

  // Graphics resources.
  ::gecko::Unique<::gecko::graphics::GraphicsDevice> m_Device;
  WindowSlot m_Slots[2] {};

  ::gecko::graphics::RenderTarget m_OffscreenRT {};
  ::gecko::graphics::Texture m_PlasmaTex[2] {};
  ::gecko::graphics::Buffer m_VertexBuffer {};
  ::gecko::graphics::Buffer m_IndirectBuffer {};
  ::gecko::graphics::Sampler m_BlitSampler {};
  ::gecko::graphics::QueryPool m_TimestampPool {};

  ::gecko::graphics::GraphicsPipeline m_TrianglePipeline {};
  ::gecko::graphics::GraphicsPipeline m_BlitPipeline {};
  ::gecko::graphics::ComputePipeline m_PlasmaPipeline {};

  ::gecko::Unique<::gecko::graphics::IGpuSampler> m_GpuSampler;

  // Event subscriptions kept alive for Run().
  ::gecko::EventSubscription m_CloseSub {};
  ::gecko::EventSubscription m_ResizeSub {};
  ::gecko::EventSubscription m_KeySub {};

  // Loop state.
  bool m_Running = true;
  ::std::chrono::steady_clock::time_point m_StartTime {};
  ::gecko::u64 m_FrameIndex = 0;
  ::gecko::u64 m_LastHudPrintNs = 0;
};

}  // namespace gecko::examples::graphics_example
