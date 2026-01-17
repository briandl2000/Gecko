#include "gecko/core/services/jobs.h"

namespace gecko {

JobHandle NullJobSystem::Submit(JobFunction job, JobPriority priority,
                                Label label) noexcept
{
  (void)priority;
  (void)label;
  if (job)
    job();
  return JobHandle {};
}

JobHandle NullJobSystem::Submit(JobFunction job, const JobHandle* dependencies,
                                u32 dependencyCount, JobPriority priority,
                                Label label) noexcept
{
  (void)dependencies;
  (void)dependencyCount;
  (void)priority;
  (void)label;
  if (job)
    job();
  return JobHandle {};
}

void NullJobSystem::Wait(JobHandle handle) noexcept
{}
void NullJobSystem::WaitAll(const JobHandle* handles, u32 count) noexcept
{}
bool NullJobSystem::IsComplete(JobHandle handle) noexcept
{
  return true;
}
u32 NullJobSystem::WorkerThreadCount() const noexcept
{
  return 0;
}
void NullJobSystem::ProcessJobs(u32 maxJobs) noexcept
{}
bool NullJobSystem::Init() noexcept
{
  return true;
}
void NullJobSystem::Shutdown() noexcept
{}

}  // namespace gecko
