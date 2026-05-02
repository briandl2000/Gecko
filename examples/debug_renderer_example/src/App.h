#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/events.h>
#include <gecko/core/services/memory.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/debug_renderer/debug_renderer_context.h>
#include <gecko/debug_renderer/debug_renderer_module.h>
#include <gecko/graphics/graphics_device.h>
#include <gecko/graphics/graphics_module.h>
#include <gecko/graphics/graphics_types.h>
#include <gecko/platform/platform_module.h>
#include <gecko/platform/window.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/standard_log_sinks.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace app::debug_renderer_example {

/// Minimal Gecko + Graphics application: one window, a small set of
/// 2D debug lines rendered straight into the swapchain backbuffer.
/// Used as the scratch ground for prototyping the future
/// debug_renderer module.
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
  class AppModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  static ::gecko::graphics::GraphicsConfig MakeGraphicsConfig() noexcept;

  bool CreateMainWindow();
  bool CreateSwapchain();
  void SubscribeEvents();

  void HandlePendingResize();
  void RenderFrame();
  void Update();

  // Services / modules / sinks. Each module owns its production
  // defaults internally via its default ctor.
  ::gecko::runtime::TrackingAllocator m_Allocator;
  ::gecko::AllocatorScope m_AllocScope {m_Allocator};

  ::gecko::runtime::RuntimeModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  ::gecko::graphics::GraphicsModule m_GraphicsModule;
  ::gecko::debug_renderer::DebugRendererModule m_DebugRendererModule;
  AppModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;
  ::std::optional<::gecko::runtime::StandardLogSinks> m_LogSinks;

  // Graphics resources -- declared after m_Engine so they are destroyed
  // before the engine tears the GraphicsModule (and its device) down.
  ::gecko::graphics::GraphicsDevice* m_Device = nullptr;

  ::gecko::platform::WindowHandle m_Window {};
  ::gecko::graphics::Swapchain m_Swapchain {};

  gecko::Shared<gecko::debug_renderer::DebugRendererContext>
      m_DebugRendererContext {};

  ::gecko::EventSubscription m_CloseSub {};
  ::gecko::EventSubscription m_ResizeSub {};
  ::gecko::EventSubscription m_KeySub {};

  bool m_Running {true};
  bool m_ResizeDirty {false};
  ::gecko::u32 m_ResizeW {0};
  ::gecko::u32 m_ResizeH {0};
};

}  // namespace app::debug_renderer_example
