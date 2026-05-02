#pragma once

/// @file
/// `GraphicsFixture` -- helper that boots a minimal Gecko engine
/// (Runtime + Platform + Graphics + DebugRenderer modules), creates
/// a window + swapchain, and exposes the live device for benchmarks.
///
/// Construct one inside a bench case's setup phase (before the
/// `for (auto _ : s)` loop):
///
/// @code
///   static void DebugLines(::gecko::bench::State& s)
///   {
///       ::gecko::bench::GraphicsFixture fx({.Title = "bench/debug_lines",
///                                           .Width = 1280, .Height = 720});
///       if (!fx.IsValid()) { s.Abort("graphics setup failed"); return; }
///
///       for (auto _ : s)
///       {
///           fx.PumpEvents();
///           if (auto frame = fx.BeginFrame(); frame.Valid)
///           {
///               // ... record + submit cmd list, present ...
///               fx.Present(frame);
///           }
///       }
///   }
/// @endcode
///
/// Owns the engine; destructor tears the engine down in the correct
/// order (resources -> swapchain -> window -> engine).

#include <gecko/core/api.h>
#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/types.h>
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

namespace gecko::bench {

/// Configuration for `GraphicsFixture`.
struct GraphicsFixtureConfig
{
  const char* Title {"gecko bench"};
  ::gecko::u32 Width {1280};
  ::gecko::u32 Height {720};
  bool Visible {true};
  bool VSync {false};
  ::gecko::u32 NumBackBuffers {2};
};

/// Boots a minimal engine with graphics + a window + a swapchain.
class GraphicsFixture
{
public:
  GECKO_API explicit GraphicsFixture(
      const GraphicsFixtureConfig& cfg = {}) noexcept;
  GECKO_API ~GraphicsFixture() noexcept;

  GraphicsFixture(const GraphicsFixture&) = delete;
  GraphicsFixture& operator=(const GraphicsFixture&) = delete;

  /// True if engine, window, and swapchain were created successfully.
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Valid;
  }

  /// Live graphics device. Null until `IsValid()`.
  [[nodiscard]] ::gecko::graphics::GraphicsDevice* Device() const noexcept
  {
    return m_Device;
  }

  /// The window's swapchain.
  [[nodiscard]] const ::gecko::graphics::Swapchain& Swapchain() const noexcept
  {
    return m_Swapchain;
  }

  /// The owning window's handle.
  [[nodiscard]] ::gecko::platform::WindowHandle Window() const noexcept
  {
    return m_Window;
  }

  /// Run one tick of OS event pumping + Gecko event dispatch. Call
  /// once per iteration before `BeginFrame`.
  GECKO_API void PumpEvents() noexcept;

  /// Begin a frame against the swapchain.
  [[nodiscard]] GECKO_API ::gecko::graphics::FrameContext BeginFrame() noexcept;

  /// Present a frame previously returned by `BeginFrame`.
  GECKO_API void Present(::gecko::graphics::FrameContext& frame) noexcept;

private:
  ::gecko::runtime::TrackingAllocator m_Allocator;
  ::gecko::AllocatorScope m_AllocScope {m_Allocator};

  ::gecko::runtime::RuntimeModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  ::gecko::graphics::GraphicsModule m_GraphicsModule;
  ::gecko::debug_renderer::DebugRendererModule m_DebugRendererModule;

  ::std::optional<::gecko::Engine> m_Engine;
  ::std::optional<::gecko::runtime::StandardLogSinks> m_LogSinks;

  ::gecko::graphics::GraphicsDevice* m_Device {nullptr};
  ::gecko::platform::WindowHandle m_Window {};
  ::gecko::graphics::Swapchain m_Swapchain {};
  bool m_Valid {false};
};

}  // namespace gecko::bench
