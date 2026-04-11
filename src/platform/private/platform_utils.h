#pragma once

#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

namespace gecko::platform {

inline u64 NowNsSafe() noexcept
{
  if (auto* profiler = GetProfiler())
    return profiler->NowNs();
  return 0;
}

}  // namespace gecko::platform
