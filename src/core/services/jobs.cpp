#include "gecko/core/services/jobs.h"

namespace gecko {

// NullJobSystem - executes jobs synchronously, no profiling overhead
JobHandle NullJobSystem::Submit(JobFunction job, JobPriority, Label) noexcept
{
  if (job)
    job();
  return JobHandle {};
}

JobHandle NullJobSystem::Submit(JobFunction job, const JobHandle*, u32,
                                JobPriority, Label) noexcept
{
  if (job)
    job();
  return JobHandle {};
}

void NullJobSystem::Wait(JobHandle) noexcept
{}
void NullJobSystem::WaitAll(const JobHandle*, u32) noexcept
{}
bool NullJobSystem::IsComplete(JobHandle) noexcept
{
  return true;
}
u32 NullJobSystem::WorkerThreadCount() const noexcept
{
  return 0;
}
void NullJobSystem::ProcessJobs(u32) noexcept
{}
bool NullJobSystem::Init() noexcept
{
  return true;
}
void NullJobSystem::Shutdown() noexcept
{}

}  // namespace gecko
