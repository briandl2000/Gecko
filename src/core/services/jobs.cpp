#include "gecko/core/services/jobs.h"

namespace gecko {

// NullJobSystem - executes jobs synchronously, no profiling overhead
static void RunAndFree(JobFn& job) noexcept
{
  if (job.Invoke)
    job.Invoke(job.User);
  // Match ThreadPoolJobSystem and the documented MakeJobFn contract:
  // only call Free when there is captured state to release.
  if (job.Free && job.User)
    job.Free(job.User);
}

JobHandle NullJobSystem::SubmitRaw(JobFn job, JobPriority, Label) noexcept
{
  RunAndFree(job);
  return JobHandle {};
}

JobHandle NullJobSystem::SubmitRaw(JobFn job, const JobHandle*, u32, JobPriority, Label) noexcept
{
  RunAndFree(job);
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
