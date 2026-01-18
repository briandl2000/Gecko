#include "gecko/core/services/profiler.h"

namespace gecko {

void NullProfiler::Emit(const ProfEvent& event) noexcept
{}
u64 NullProfiler::NowNs() const noexcept
{
  return 0;
}
void NullProfiler::AddSinkImpl(IProfilerSink* sink) noexcept
{}
void NullProfiler::RemoveSinkImpl(IProfilerSink* sink) noexcept
{}
bool NullProfiler::Init() noexcept
{
  return true;
}
void NullProfiler::Shutdown() noexcept
{}

}  // namespace gecko
