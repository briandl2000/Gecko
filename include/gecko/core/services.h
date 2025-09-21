#pragma once
#include "api.h"
#include "profiler.h"

namespace gecko {
  
  struct Services
  {
    IProfiler* profiler = nullptr;
  };

  GECKO_API void InstallServices(const Services&) noexcept;

  GECKO_API IProfiler* Profiler() noexcept;

  GECKO_API const Services& services() noexcept;
}
