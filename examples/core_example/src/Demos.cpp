#include "Demos.h"

#include "Labels.h"

namespace gecko::examples::core_example::demos {

namespace {

struct Particle
{
  float X, Y, Z;
  float VelocityX, VelocityY, VelocityZ;
};

void RunParticleWorker(i32 workerId, i32 particleCount) noexcept
{
  GECKO_PROFILE_NAMED(labels::Worker, "ParticleWorker");
  GECKO_INFO(labels::Worker, "worker {}: {} particles", workerId, particleCount);

  Particle* particles = AllocArray<Particle>(static_cast<u64>(particleCount));
  if (particles == nullptr)
    return;

  SeedRandom(static_cast<u64>(workerId * 12345 + 42));
  for (i32 index = 0; index < particleCount; ++index)
  {
    particles[index] = {
        RandomF32(-100.0F, 100.0F), RandomF32(-100.0F, 100.0F), RandomF32(-100.0F, 100.0F),
        RandomF32(-10.0F, 10.0F),   RandomF32(-10.0F, 10.0F),   RandomF32(-10.0F, 10.0F),
    };
  }

  for (i32 step = 0; step < 50; ++step)
  {
    GECKO_PROFILE_NAMED(labels::Compute, "PhysicsStep");
    for (i32 index = 0; index < particleCount; ++index)
    {
      Particle& particle = particles[index];
      particle.X += particle.VelocityX * 0.016F;
      particle.Y += particle.VelocityY * 0.016F;
      particle.Z += particle.VelocityZ * 0.016F;
      particle.VelocityX *= 0.999F;
      particle.VelocityY *= 0.999F;
      particle.VelocityZ *= 0.999F;
    }
  }
  DeallocBytes(particles);
}

struct EventDemoState
{
  SpinMutex Mutex;
  u32 ImmediateCount {0};
  u32 QueuedCount {0};
};

void OnEventImmediate(void* user, const EventMeta&, EventView payload) noexcept
{
  auto* state = static_cast<EventDemoState*>(user);
  const auto* event = static_cast<const events::TestEventPayload*>(payload.Data());
  LockGuard lock {state->Mutex};
  ++state->ImmediateCount;
  GECKO_INFO(labels::Events, "immediate: value={}", event != nullptr ? event->Value : 0U);
}

void OnEventQueued(void* user, const EventMeta&, EventView payload) noexcept
{
  auto* state = static_cast<EventDemoState*>(user);
  const auto* event = static_cast<const events::TestEventPayload*>(payload.Data());
  LockGuard lock {state->Mutex};
  ++state->QueuedCount;
  GECKO_INFO(labels::Events, "queued: value={}", event != nullptr ? event->Value : 0U);
}

}  // namespace

void RunMemory() noexcept
{
  GECKO_PROFILE(labels::Memory);
  struct Allocation
  {
    void* Memory {nullptr};
    usize Size {0};
  };
  Allocation allocations[50] {};
  const MemoryStats before = GetMemoryStats();

  for (usize index = 0; index < 50; ++index)
  {
    allocations[index].Size = random::Size(64, 4096);
    allocations[index].Memory = AllocBytes(allocations[index].Size, index < 25 ? 16 : 32);
    if (allocations[index].Memory != nullptr)
      MemorySet(allocations[index].Memory, static_cast<u8>(index), allocations[index].Size);
  }
  const MemoryStats during = GetMemoryStats();
  GECKO_COUNTER(labels::Memory, "LiveBytes", during.LiveBytes);
  GECKO_INFO(labels::Memory, "allocated {} blocks; live bytes {} -> {}",
             during.AllocationCount - before.AllocationCount, before.LiveBytes, during.LiveBytes);

  for (auto& allocation : allocations)
    DeallocBytes(allocation.Memory);
  const MemoryStats after = GetMemoryStats();
  GECKO_INFO(labels::Memory, "after free: live={} peak={}", after.LiveBytes, after.PeakBytes);
}

void RunEvents() noexcept
{
  GECKO_PROFILE(labels::Events);
  (void)RegisterEventModule(labels::App.Id);
  EventDemoState state {};
  EventSubscription immediate = SubscribeEvent(events::TestEvent, OnEventImmediate, &state,
                                               SubscriptionOptions {.Delivery = SubscriptionDelivery::Immediate});
  EventSubscription queued = SubscribeEvent(events::TestEvent, OnEventQueued, &state);
  const EventEmitter emitter = CreateEmitterForModule(labels::App, 0xC0DE);

  SendEvent(emitter, events::TestEvent, events::TestEventPayload {.Value = 1});
  const JobHandle job =
      SubmitJob([emitter]() { SendEvent(emitter, events::TestEvent, events::TestEventPayload {.Value = 2}); },
                JobPriority::Normal, labels::Worker);
  WaitForJob(job);
  SendEvent(emitter, events::TestEvent, events::TestEventPayload {.Value = 3});

  GECKO_INFO(labels::Events, "before dispatch: immediate={} queued={}", state.ImmediateCount, state.QueuedCount);
  const usize dispatched = DispatchEvents();
  GECKO_INFO(labels::Events, "dispatched {}; immediate={} queued={}", dispatched, state.ImmediateCount,
             state.QueuedCount);
  immediate.Reset();
  queued.Reset();
  UnregisterEventModule(labels::App.Id);
}

void RunThreading() noexcept
{
  GECKO_PROFILE(labels::Main);
  GECKO_INFO(labels::Main, "thread={} hardware threads={}", ThisThreadId(), HardwareThreadCount());
  const u64 before = MonotonicTimeNs();
  PreciseSleepNs(1'000'000);
  GECKO_INFO(labels::Main, "precise 1ms sleep measured={}ns", MonotonicTimeNs() - before);
  YieldThread();
  const u64 sleepBefore = MonotonicTimeNs();
  SleepMs(10);
  GECKO_INFO(labels::Main, "10ms sleep measured={:.3}ms", time::NsToMillisecondsF(MonotonicTimeNs() - sleepBefore));
}

void RunJobs() noexcept
{
  GECKO_PROFILE(labels::Main);
  JobHandle simulations[3] {};
  for (i32 index = 0; index < 3; ++index)
  {
    const i32 particleCount = 800 + index * 400;
    simulations[index] = SubmitJob([index, particleCount]() { RunParticleWorker(index, particleCount); },
                                   JobPriority::Normal, labels::Worker);
  }
  WaitForJobs(simulations, 3);

  GECKO_INFO(labels::Main, "pipeline: stage1 -> stage2 -> stage3");
  const JobHandle stage1 = SubmitJob([]() { SleepMs(4); }, JobPriority::High, labels::Compute);
  const JobHandle stage2 = SubmitJob([]() { SleepMs(5); }, &stage1, 1, JobPriority::Normal, labels::Compute);
  const JobHandle stage3 = SubmitJob([]() { SleepMs(3); }, &stage2, 1, JobPriority::Normal, labels::Compute);
  while (!IsJobComplete(stage3))
    ProcessJobs(1);
}

void RunLogging() noexcept
{
  GECKO_TRACE(labels::Main, "trace: very detailed");
  GECKO_DEBUG(labels::Main, "debug: development information");
  GECKO_INFO(labels::Main, "info: general information");
  GECKO_WARN(labels::Main, "warn: something might be off");
  GECKO_ERROR(labels::Main, "error: demonstration only");
}

void RunProfilerDiagnostics() noexcept
{
  const ProfilerDiagnostics diagnostics = GetProfilerDiagnostics();
  GECKO_INFO(labels::Main, "profiler diagnostics: dropped={} open-overflow={} stats-overflow={}",
             diagnostics.DroppedEvents, diagnostics.OpenZoneOverflow, diagnostics.StatsOverflow);
}

}  // namespace gecko::examples::core_example::demos
