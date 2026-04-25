#pragma once

#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::platform {

namespace labels {
inline constexpr ::gecko::Label Platform = ::gecko::MakeLabel("gecko.platform");
}

// Platform library's module. Stack-construct one and pass &platform into
// Engine::Create({...}). The platform module currently owns no state of
// its own; it exists as a lifecycle node for one-shot platform setup
// (Win32 multimedia timer resolution today, more later).
//
// Filesystem and threading APIs are stateless namespace functions
// (`gecko::platform::Read`, `gecko::platform::HardwareThreadCount`, ...)
// not services — see copilot_context/MODULE_API_SHAPING.md.
class PlatformModule final : public ::gecko::IModule
{
public:
  PlatformModule() noexcept;
  ~PlatformModule() noexcept override;

  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override
  {
    return labels::Platform;
  }

  // Startup emits diagnostics via GECKO_INFO/WARN/FUNC. Declare the
  // dependencies so the registry's topological sort wires up logging
  // before us.
  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::platform
