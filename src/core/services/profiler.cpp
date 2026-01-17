#include "gecko/core/services/profiler.h"

namespace gecko {

void NullProfiler::Emit(const ProfEvent& event) noexcept
{}
u64 NullProfiler::NowNs() const noexcept
{
  return 0;
}
bool NullProfiler::Init() noexcept
{
  return true;
}
void NullProfiler::Shutdown() noexcept
{}

}  // namespace gecko
