#pragma once

/// @file
/// `GraphicsModule` engine module.
///
/// The graphics module emits startup/shutdown diagnostics and depends
/// on the foundational services so the topological sort orders it
/// after the services-publisher (and, transitively, after
/// `PlatformModule`).

#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::graphics {

/// Engine module for the graphics library. Stack-construct one and
/// pass it to `Engine::Create({...})`.
class GraphicsModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override;

  /// Foundational services this module depends on. Used by the
  /// module-registry topological sort so graphics initializes after
  /// the services publisher and `PlatformModule`.
  [[nodiscard]] GECKO_API ::gecko::Span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::graphics
