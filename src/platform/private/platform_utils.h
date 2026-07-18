#pragma once

#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

namespace gecko::platform {

inline u64 NowNsSafe() noexcept
{
  return ProfilerNowNs();
}

}  // namespace gecko::platform
