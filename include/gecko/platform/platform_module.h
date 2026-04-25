#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::platform {

struct IThreading;

namespace labels {
inline constexpr ::gecko::Label Platform = ::gecko::MakeLabel("gecko.platform");
}

// Platform library's module. Stack-construct one and pass &platform into
// Engine::Create({...}). Owns and publishes the platform-provided
// services (IThreading today, IPlatformIO next) and is the lifecycle
// node for platform-wide setup such as Win32 timer resolution.
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

  // Platform startup/shutdown emits diagnostics via GECKO_INFO/WARN/FUNC
  // and may submit jobs / events. Declare the dependency so the
  // registry's topological sort starts the services-publisher first.
  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Publishes()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;

private:
  ::gecko::Unique<IThreading> m_Threading;
};

}  // namespace gecko::platform
