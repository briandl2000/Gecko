#pragma once
#include "api.h"
#include "profiler.h"
#include "memory.h"

namespace gecko {
  
  struct Services
  {
    IAllocator* Allocator = nullptr;
    IProfiler* Profiler = nullptr;
  };

  GECKO_API void InstallServices(const Services& service) noexcept;

  GECKO_API void UninstallServices() noexcept;


  GECKO_API IProfiler* GetProfiler() noexcept;
  GECKO_API IAllocator* GetAllocator() noexcept;

  bool IsServicesInstalled() noexcept;

  bool ValidateServices(bool fatalOnFail) noexcept;

}
#ifndef GECKO_REQUIRE_INSTALL
  #define GECKO_REQUIRE_INSTALL 1
#endif
