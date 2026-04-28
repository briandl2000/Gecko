#include "Demos.h"

#include "Labels.h"

#include <atomic>
#include <cstring>
#include <gecko/core/scope.h>
#include <gecko/core/services.h>
#include <gecko/core/services/events.h>
#include <gecko/core/services/log.h>
#include <gecko/core/services/memory.h>
#include <gecko/core/utility/random.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/utility/time.h>
#include <gecko/runtime/tracking_allocator.h>
#include <utility>
#include <vector>

namespace gecko::examples::core_example::demos {

namespace {

// ── Memory demo helpers ─────────────────────────────────────────────

void PrintMemoryStats(const ::gecko::runtime::TrackingAllocator& tracker)
{
  GECKO_INFO(labels::Main, "Total Live Bytes: %llu", tracker.TotalLiveBytes());

  const ::gecko::Label all[] = {labels::Main, labels::Worker, labels::Memory,
                                labels::Compute, labels::Simulation};

  ::gecko::u64 totalAllocs = 0;
  ::gecko::u64 totalFrees = 0;
  ::gecko::u64 totalLive = 0;

  for (auto label : all)
  {
    ::gecko::runtime::MemLabelStats stats;
    if (!tracker.StatsFor(label, stats))
      continue;

    const ::gecko::u64 live = stats.LiveBytes.load();
    const ::gecko::u64 allocs = stats.Allocs.load();
    const ::gecko::u64 frees = stats.Frees.load();
    totalAllocs += allocs;
    totalFrees += frees;
    totalLive += live;

    GECKO_INFO(labels::Main, "Label '%s': Live=%llu Allocs=%llu Frees=%llu",
               label.Name ? label.Name : "(unnamed)", live, allocs, frees);
  }

  GECKO_INFO(labels::Main, "Summary: %llu allocs / %llu frees / %llu live",
             totalAllocs, totalFrees, totalLive);
}

// ── Particle simulation (used by the job system demo) ───────────────

struct Particle
{
  float x, y, z;
  float vx, vy, vz;
  float mass;
};

void RunParticleWorker(int workerId, int numParticles)
{
  GECKO_SCOPE(labels::Worker);
  GECKO_INFO(labels::Worker, "Worker %d: %d particles", workerId, numParticles);

  Particle* particles = nullptr;
  {
    GECKO_SCOPE_NAMED(labels::Memory, "AllocateParticles");
    particles = ::gecko::AllocArray<Particle>(numParticles);
  }
  if (!particles)
  {
    GECKO_ERROR(labels::Worker, "Worker %d: alloc failed", workerId);
    return;
  }

  {
    GECKO_SCOPE_NAMED(labels::Compute, "InitializeParticles");
    ::gecko::SeedRandom(workerId * 12345 + 42);
    for (int i = 0; i < numParticles; ++i)
    {
      particles[i] = {::gecko::RandomF32(-100.0f, 100.0f),
                      ::gecko::RandomF32(-100.0f, 100.0f),
                      ::gecko::RandomF32(-100.0f, 100.0f),
                      ::gecko::RandomF32(-10.0f, 10.0f),
                      ::gecko::RandomF32(-10.0f, 10.0f),
                      ::gecko::RandomF32(-10.0f, 10.0f),
                      ::gecko::RandomF32(1.0f, 5.0f)};
    }
  }

  {
    GECKO_SCOPE_NAMED(labels::Compute, "PhysicsUpdate");
    constexpr int kSteps = 50;
    constexpr float kDt = 0.016f;
    constexpr float kDamping = 0.999f;
    for (int step = 0; step < kSteps; ++step)
    {
      GECKO_SCOPE_NAMED(labels::Compute, "PhysicsStep");
      for (int i = 0; i < numParticles; ++i)
      {
        Particle& p = particles[i];
        p.x += p.vx * kDt;
        p.y += p.vy * kDt;
        p.z += p.vz * kDt;
        p.vx *= kDamping;
        p.vy *= kDamping;
        p.vz *= kDamping;
      }
    }
  }

  {
    GECKO_SCOPE_NAMED(labels::Memory, "DeallocateParticles");
    ::gecko::DeallocBytes(particles);
  }
}

// ── Event demo helpers ──────────────────────────────────────────────

struct EventDemoState
{
  ::std::atomic<::gecko::u32> immediateCount {0};
  ::std::atomic<::gecko::u32> queuedCount {0};
  ::gecko::EventSubscription immediateSub {};
  ::gecko::EventSubscription queuedSub {};
};

void OnEventImmediate(void* user, const ::gecko::EventMeta&,
                      ::gecko::EventView payload)
{
  auto* state = static_cast<EventDemoState*>(user);
  const auto* p = static_cast<const events::TestEventPayload*>(payload.Data());
  state->immediateCount.fetch_add(1, ::std::memory_order_relaxed);
  GECKO_INFO(labels::Events, "Immediate: value=%u", p ? p->value : 0u);
}

void OnEventQueued(void* user, const ::gecko::EventMeta&,
                   ::gecko::EventView payload)
{
  auto* state = static_cast<EventDemoState*>(user);
  const auto* p = static_cast<const events::TestEventPayload*>(payload.Data());
  state->queuedCount.fetch_add(1, ::std::memory_order_relaxed);
  GECKO_INFO(labels::Events, "Queued: value=%u", p ? p->value : 0u);
}

}  // namespace

void RunMemory(::gecko::runtime::TrackingAllocator& tracker)
{
  GECKO_SCOPE(labels::Memory);

  ::std::vector<::std::pair<void*, ::std::size_t>> allocations;

  for (int i = 0; i < 25; ++i)
  {
    const ::std::size_t size = ::gecko::random::Size(64, 4096);
    if (void* ptr = ::gecko::AllocBytes(size, 16))
    {
      allocations.push_back({ptr, size});
      ::std::memset(ptr, i & 0xFF, size);
    }
    if (i % 10 == 0)
    {
      GECKO_COUNTER(labels::Memory, "LiveBytes", tracker.TotalLiveBytes());
    }
  }

  for (::std::size_t i = 0; i < allocations.size(); ++i)
  {
    const ::std::size_t swapIndex = ::gecko::random::Index(allocations.size());
    ::std::swap(allocations[i], allocations[swapIndex]);
  }

  for (::std::size_t i = 0; i < allocations.size() / 2; ++i)
  {
    if (allocations[i].first)
    {
      ::gecko::DeallocBytes(allocations[i].first);
      allocations[i].first = nullptr;
    }
  }

  for (int i = 0; i < 25; ++i)
  {
    const ::std::size_t size = ::gecko::random::Size(64, 4096);
    if (void* ptr = ::gecko::AllocBytes(size, 32))
    {
      allocations.push_back({ptr, size});
      ::std::memset(ptr, (i + 100) & 0xFF, size);
    }
  }

  for (auto& alloc : allocations)
  {
    if (alloc.first)
      ::gecko::DeallocBytes(alloc.first);
  }

  GECKO_COUNTER(labels::Memory, "FinalLiveBytes", tracker.TotalLiveBytes());
  PrintMemoryStats(tracker);
}

void RunEvents()
{
  GECKO_SCOPE(labels::Events);

  EventDemoState state {};

  state.immediateSub = ::gecko::SubscribeEvent(
      events::TestEvent, &OnEventImmediate, &state,
      ::gecko::SubscriptionOptions {
          .delivery = ::gecko::SubscriptionDelivery::Immediate});

  state.queuedSub = ::gecko::SubscribeEvent(
      events::TestEvent, &OnEventQueued, &state,
      ::gecko::SubscriptionOptions {.delivery =
                                        ::gecko::SubscriptionDelivery::Queued});

  const ::gecko::EventEmitter emitter =
      ::gecko::CreateEmitterForModule(labels::App, /*sender=*/0xC0DE);

  // Three Send calls — two from main, one from a worker job.
  {
    events::TestEventPayload payload {.value = 1};
    ::gecko::SendEvent(emitter, events::TestEvent, payload);
  }
  ::gecko::JobHandle job = ::gecko::SubmitJob(
      [emitter]() {
        events::TestEventPayload payload {.value = 2};
        ::gecko::SendEvent(emitter, events::TestEvent, payload);
      },
      ::gecko::JobPriority::Normal, labels::Worker);
  ::gecko::WaitForJob(job);
  {
    events::TestEventPayload payload {.value = 3};
    ::gecko::SendEvent(emitter, events::TestEvent, payload);
  }

  GECKO_INFO(labels::Events, "Before Dispatch: Immediate=%u Queued=%u",
             state.immediateCount.load(), state.queuedCount.load());

  const ::std::size_t dispatched = ::gecko::DispatchEvents();
  GECKO_INFO(labels::Events, "Dispatched %zu queued events", dispatched);
  GECKO_INFO(labels::Events, "After Dispatch:  Immediate=%u Queued=%u",
             state.immediateCount.load(), state.queuedCount.load());
}

void RunThreading()
{
  GECKO_SCOPE(labels::Main);

  GECKO_INFO(labels::Main, "ThisThreadId=%u  HardwareThreadCount=%u",
             ::gecko::ThisThreadId(), ::gecko::HardwareThreadCount());

  const ::gecko::u64 t0 = ::gecko::HighResTimeNs();
  ::gecko::PreciseSleepNs(1'000'000);  // 1 ms
  GECKO_INFO(labels::Main, "PreciseSleepNs(1ms) measured=%llu ns",
             ::gecko::HighResTimeNs() - t0);

  ::gecko::YieldThread();

  const ::gecko::u64 t1 = ::gecko::HighResTimeNs();
  ::gecko::SleepMs(10);
  GECKO_INFO(labels::Main, "SleepMs(10) measured=%llu ms",
             ::gecko::time::NsToMilliseconds(::gecko::HighResTimeNs() - t1));
}

void RunJobs(::gecko::runtime::TrackingAllocator& tracker)
{
  GECKO_SCOPE(labels::Main);

  // Parallel particle simulations.
  constexpr int kWorkers = 3;
  ::std::vector<::gecko::JobHandle> sims;
  for (int i = 0; i < kWorkers; ++i)
  {
    const int particleCount = 800 + i * 400;
    auto job = [i, particleCount]() { RunParticleWorker(i, particleCount); };
    sims.push_back(
        ::gecko::SubmitJob(job, ::gecko::JobPriority::Normal, labels::Worker));
  }
  ::gecko::WaitForJobs(sims.data(), static_cast<::gecko::u32>(sims.size()));
  PrintMemoryStats(tracker);
  tracker.EmitCounters();

  // 3-stage dependency pipeline.
  GECKO_INFO(labels::Main, "Pipeline: Stage1 -> Stage2 -> Stage3");
  auto stage1 = ::gecko::SubmitJob(
      []() {
        GECKO_SCOPE_NAMED(labels::Compute, "PipelineStage1");
        ::gecko::SleepMs(40);
      },
      ::gecko::JobPriority::High, labels::Compute);

  ::gecko::JobHandle deps1[] = {stage1};
  auto stage2 = ::gecko::SubmitJob(
      []() {
        GECKO_SCOPE_NAMED(labels::Compute, "PipelineStage2");
        ::gecko::SleepMs(50);
      },
      deps1, 1, ::gecko::JobPriority::Normal, labels::Compute);

  ::gecko::JobHandle deps2[] = {stage2};
  auto stage3 = ::gecko::SubmitJob(
      []() {
        GECKO_SCOPE_NAMED(labels::Compute, "PipelineStage3");
        ::gecko::SleepMs(35);
      },
      deps2, 1, ::gecko::JobPriority::Normal, labels::Compute);

  while (!::gecko::IsJobComplete(stage3))
    ::gecko::GetJobSystem()->ProcessJobs(1);

  // Main-thread job processing.
  GECKO_INFO(labels::Main, "Processing low-priority jobs on main thread");
  ::std::vector<::gecko::JobHandle> mainJobs;
  for (int i = 0; i < 3; ++i)
  {
    auto job = [i]() {
      GECKO_SCOPE_NAMED(labels::Compute, "MainThreadJob");
      GECKO_INFO(labels::Compute, "main-thread job %d", i);
    };
    mainJobs.push_back(
        ::gecko::SubmitJob(job, ::gecko::JobPriority::Low, labels::Compute));
  }
  ::gecko::GetJobSystem()->ProcessJobs(2);
  ::gecko::WaitForJobs(mainJobs.data(),
                       static_cast<::gecko::u32>(mainJobs.size()));
}

void RunLogging()
{
  GECKO_TRACE(labels::Main, "trace: very detailed");
  GECKO_DEBUG(labels::Main, "debug: development info");
  GECKO_INFO(labels::Main, "info: general information");
  GECKO_WARN(labels::Main, "warn: something might be off");
  GECKO_ERROR(labels::Main, "error: something went wrong (test only)");
}

void RunProfilerDiagnostics()
{
  if (auto* profiler = ::gecko::GetProfiler())
  {
    auto diag = profiler->GetDiagnostics();
    GECKO_INFO(labels::Main,
               "Profiler diagnostics: dropped=%llu reentrant=%llu "
               "agg_overflow=%llu",
               static_cast<unsigned long long>(diag.DroppedEvents),
               static_cast<unsigned long long>(diag.ReentrantDrops),
               static_cast<unsigned long long>(diag.AggregatorOverflow));
  }
}

}  // namespace gecko::examples::core_example::demos
