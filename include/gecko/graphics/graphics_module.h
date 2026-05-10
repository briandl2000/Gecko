#pragma once

/// @file
/// `GraphicsModule` -- the engine module that owns the `GraphicsDevice`
/// and `IGpuSampler` and publishes them as services.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/modules.h"
#include "gecko/graphics/gpu_profiler.h"
#include "gecko/graphics/graphics_device.h"

namespace gecko::graphics {

namespace labels {
/// Module label used by the graphics module.
inline constexpr ::gecko::Label Graphics = ::gecko::MakeLabel("gecko.graphics");
}  // namespace labels

/// Configuration for `GraphicsModule`.
///
/// Mirrors `GraphicsDeviceDesc` but adds an `Auto` backend (resolved
/// during `Startup`) and a `PreferredGpu` hint. Reserved for future
/// device-recreation / hot-swap workflows -- the module API is shaped
/// so that `Recreate(newConfig)` can be added without breaking
/// callers, since service consumers re-fetch via `GetGraphicsDevice()`
/// every time they need it.
struct GraphicsConfig
{
  /// Which backend to instantiate. `Null` for headless tests; explicit
  /// `Vulkan` for production. (`Auto` resolution lives at the example
  /// layer for now and may move here later.)
  GraphicsBackend Backend {GraphicsBackend::Null};

  /// Enable backend validation (Vulkan validation layers, etc.).
  bool Debug {false};

  /// Application name reported to the driver.
  const char* AppName {"Gecko"};

  /// Hint for which physical GPU to prefer. Forward-looking;
  /// currently consumed only by backends that implement device
  /// selection. Non-owning -- caller keeps the string alive for the
  /// lifetime of the module.
  const char* PreferredGpu {nullptr};

  /// `GpuSamplerDesc` used to create the published `IGpuSampler`.
  GpuSamplerDesc Sampler {};
};

/// Engine module for the graphics library.
///
/// During `Startup()` the module:
/// - Creates the `GraphicsDevice` (if not injected via `Backends`).
/// - Creates the `IGpuSampler` from the device. Some backends
///   (`NullDevice`) return null -- the sampler service is then simply
///   not published.
/// - Publishes `GraphicsDevice` and (when available) `IGpuSampler`.
///
/// Stack-construct one and pass it to `Engine::Create({...})`.
class GraphicsModule final : public ::gecko::IModule
{
public:
  /// Optional backend injection for tests and specialised hosts.
  /// Non-null fields override the corresponding default; the caller
  /// keeps ownership and must outlive the module.
  struct Backends
  {
    GraphicsDevice* Device = nullptr;
  };

  /// Construct with default config + module-owned backends.
  GECKO_API explicit GraphicsModule(const GraphicsConfig& config = {}) noexcept;

  /// Construct with custom backends. Any null field is auto-created
  /// from `config` during `Startup`.
  GECKO_API GraphicsModule(const GraphicsConfig& config, Backends backends) noexcept;

  GECKO_API ~GraphicsModule() noexcept override;

  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel() const noexcept override
  {
    return labels::Graphics;
  }

  [[nodiscard]] GECKO_API ::gecko::Span<const ::gecko::ServiceId> Requires() const noexcept override;

  [[nodiscard]] GECKO_API ::gecko::Span<const ::gecko::ServiceId> Publishes() const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;

  /// Resolved config used to construct the device.
  [[nodiscard]] GECKO_API const GraphicsConfig& Config() const noexcept
  {
    return m_Config;
  }

private:
  GraphicsConfig m_Config;

  // Externally-owned (injected) device. Null unless the user passed
  // it via the Backends ctor.
  GraphicsDevice* m_Device = nullptr;

  // Internally-owned defaults populated during Startup().
  ::gecko::Unique<GraphicsDevice> m_OwnedDevice;
  ::gecko::Unique<IGpuSampler> m_OwnedSampler;
};

// Service accessors ------------------------------------------------
//
// Available between `GraphicsModule::Startup` and `Shutdown`. Always
// re-fetch -- callers must not cache the returned pointer; this is
// the hook for future device-recreation support.

/// Get the active `GraphicsDevice`, or `nullptr` if not started.
[[nodiscard]] GECKO_API GraphicsDevice* GetGraphicsDevice() noexcept;

/// Get the active `IGpuSampler`, or `nullptr` if not started or if
/// the active backend has no GPU timestamp support (e.g. NullDevice).
[[nodiscard]] GECKO_API IGpuSampler* GetGpuSampler() noexcept;

}  // namespace gecko::graphics
