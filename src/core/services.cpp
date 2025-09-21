#include <atomic>
#include <cstdint>

#include "gecko/core/services.h"
#include "gecko/core/category.h"

namespace {
  std::atomic<::gecko::IProfiler*> g_Profiler{nullptr}; 
  gecko::Services g_Services {};
}

namespace gecko::categories {
  inline constexpr auto core = MakeCategory("core");
}

namespace gecko { 
  void InstallServices(const Services& s) noexcept {
    g_Services = s;
    g_Profiler.store(s.profiler, std::memory_order_release);
  }

  IProfiler* GetProfiler() noexcept { return g_Profiler.load(std::memory_order_acquire); }
  const Services& services() noexcept { return g_Services; }
}
