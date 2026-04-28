#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/ptr.h>
#include <gecko/core/services.h>
#include <gecko/core/services/events.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/graphics/graphics_device.h>
#include <gecko/graphics/graphics_types.h>
#include <gecko/platform/platform_module.h>
#include <gecko/platform/window.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/ring_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace app::debug_renderer_example {

/// Minimal Gecko + Graphics application: one window, a single coloured
/// triangle rendered straight into the swapchain backbuffer. Used as the
/// scratch ground for prototyping the future debug_renderer module.
class App
{
public:
  App();
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value() && m_Device != nullptr && m_Window.IsValid();
  }

  int Run();

private:
  struct AllocatorInstaller
  {
    explicit AllocatorInstaller(::gecko::IAllocator* a) noexcept;
    ~AllocatorInstaller();
    AllocatorInstaller(const AllocatorInstaller&) = delete;
    AllocatorInstaller& operator=(const AllocatorInstaller&) = delete;
    bool Ok = false;
  };

  class AppModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  void AttachSinks();
  void DetachSinks();

  bool CreateMainWindow();
  bool CreateDevice();
  bool CreateSwapchain();
  bool CreateRenderResources();
  bool CreatePipeline();
  void SubscribeEvents();

  void HandlePendingResize();
  void RenderFrame();
  void Update();

  ::gecko::runtime::TrackingAllocator m_Allocator;
  AllocatorInstaller m_AllocatorInstaller {&m_Allocator};

  ::gecko::runtime::ThreadPoolJobSystem m_JobSystem;
  ::gecko::runtime::RingProfiler m_Profiler {1 << 16};
  ::gecko::runtime::RingLogger m_Logger {1024};
  ::gecko::runtime::EventBus m_EventBus;

  ::gecko::runtime::CoreServicesModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  AppModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;

  ::gecko::runtime::ConsoleLogSink m_ConsoleSink;
  bool m_SinksAttached {false};

  ::gecko::platform::WindowHandle m_Window {};
  ::gecko::Unique<::gecko::graphics::GraphicsDevice> m_Device;
  ::gecko::graphics::Swapchain m_Swapchain {};

  ::gecko::graphics::Buffer m_VertexBuffer {};
  ::gecko::graphics::GraphicsPipeline m_DebugLinePipeline {};
  ::gecko::u32 m_LineCount {0};

  ::gecko::EventSubscription m_CloseSub {};
  ::gecko::EventSubscription m_ResizeSub {};
  ::gecko::EventSubscription m_KeySub {};

  bool m_Running {true};
  bool m_ResizeDirty {false};
  ::gecko::u32 m_ResizeW {0};
  ::gecko::u32 m_ResizeH {0};
};

}  // namespace app::debug_renderer_example
