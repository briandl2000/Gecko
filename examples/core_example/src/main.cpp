#include "gecko/core/boot.h"
#include "gecko/core/scope.h"
#include "gecko/core/services.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/utility/random.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"
#include "gecko/core/version.h"
#include "gecko/runtime/console_log_sink.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/file_log_sink.h"
#include "gecko/runtime/module_registry.h"
#include "gecko/runtime/ring_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/runtime_module.h"
#include "gecko/runtime/thread_pool_job_system.h"
#include "gecko/runtime/trace_file_sink.h"
#include "gecko/runtime/tracking_allocator.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace gecko;

namespace app::core_example::labels {
inline constexpr ::gecko::Label App = ::gecko::MakeLabel("app.core_example");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.core_example.main");
inline constexpr ::gecko::Label Events =
    ::gecko::MakeLabel("app.core_example.events");
inline constexpr ::gecko::Label Worker =
    ::gecko::MakeLabel("app.core_example.worker");
inline constexpr ::gecko::Label Memory =
    ::gecko::MakeLabel("app.core_example.memory");
inline constexpr ::gecko::Label Compute =
    ::gecko::MakeLabel("app.core_example.compute");
inline constexpr ::gecko::Label Simulation =
    ::gecko::MakeLabel("app.core_example.simulation");
}  // namespace app::core_example::labels

namespace {

class CoreExampleAppModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] ::gecko::Label RootLabel() const noexcept override
  {
    return app::core_example::labels::App;
  }

  [[nodiscard]] bool Startup(
      ::gecko::IModuleRegistry& /*modules*/) noexcept override
  {
    return true;
  }

  void Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept override
  {}
};

CoreExampleAppModule g_AppModule;

}  // namespace

// Define labels for different subsystems
// Simple data structure for our simulation
struct Particle
{
  float x, y, z;
  float vx, vy, vz;
  float mass;
};

namespace app::core_example::events {
// Use the module's Label ID for event scoping
constexpr ::gecko::Label ModuleLabel = app::core_example::labels::App;
constexpr EventCode TestEvent = MakeEventCode(ModuleLabel.Id, 0x0001);

struct TestEventPayload
{
  u32 value {0};
};
}  // namespace app::core_example::events

// Worker function that simulates some computation
void WorkerTask(int workerId, int numParticles)
{
  GECKO_FUNC(app::core_example::labels::Worker);

  GECKO_INFO(app::core_example::labels::Worker,
             "Worker %d: Starting simulation with %d particles", workerId,
             numParticles);

  // Allocate particles
  Particle* particles = nullptr;
  {
    GECKO_SCOPE_NAMED(app::core_example::labels::Memory, "AllocateParticles");
    particles = AllocArray<Particle>(numParticles);
  }
  {
    if (!particles)
    {
      GECKO_ERROR(app::core_example::labels::Worker,
                  "Worker %d: Failed to allocate particles", workerId);
      return;
    }

    GECKO_DEBUG(app::core_example::labels::Memory,
                "Worker %d: Allocated %zu bytes for %d particles", workerId,
                numParticles * sizeof(Particle), numParticles);

    // Initialize particles
    {
      GECKO_SCOPE_NAMED(app::core_example::labels::Compute,
                        "InitializeParticles");
      // Seed random generator differently per worker for unique particle
      // behaviors
      gecko::SeedRandom(workerId * 12345 + 42);

      for (int i = 0; i < numParticles; ++i)
      {
        particles[i] = {gecko::RandomF32(-100.0f, 100.0f),
                        gecko::RandomF32(-100.0f, 100.0f),
                        gecko::RandomF32(-100.0f, 100.0f),
                        gecko::RandomF32(-10.0f, 10.0f),
                        gecko::RandomF32(-10.0f, 10.0f),
                        gecko::RandomF32(-10.0f, 10.0f),
                        gecko::RandomF32(1.0f, 5.0f)};
      }
    }
  }

  {
    GECKO_SCOPE_NAMED(app::core_example::labels::Compute, "PhysicsUpdate");

    // Simulate physics steps (engaging but quick)
    const int numSteps = 50;
    const float deltaTime = 0.016f;  // Cache constant
    const float damping = 0.999f;    // Cache constant

    for (int step = 0; step < numSteps; ++step)
    {
      GECKO_SCOPE_NAMED(app::core_example::labels::Compute, "PhysicsStep");

      // Optimized physics update with better cache usage
      for (int i = 0; i < numParticles; ++i)
      {
        Particle& p = particles[i];  // Reference for better cache usage

        // Update position
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;
        p.z += p.vz * deltaTime;

        // Apply damping
        p.vx *= damping;
        p.vy *= damping;
        p.vz *= damping;
      }
    }

    GECKO_TRACE(app::core_example::labels::Compute,
                "Worker %d: Completed %d physics steps", workerId, numSteps);
    GECKO_INFO(app::core_example::labels::Worker,
               "Worker %d: Physics simulation complete! All particles settled.",
               workerId);
  }

  // Clean up
  {
    GECKO_SCOPE_NAMED(app::core_example::labels::Memory, "DeallocateParticles");
    DeallocBytes(particles);
    GECKO_DEBUG(app::core_example::labels::Memory,
                "Worker %d: Freed particle memory", workerId);
  }
}

// Function to perform some memory stress testing
void MemoryStressTest()
{
  GECKO_FUNC(app::core_example::labels::Memory);

  GECKO_INFO(app::core_example::labels::Memory, "Starting memory stress test");

  std::vector<std::pair<void*, size_t>> allocations;  // Store pointer and size

  // Get the tracking allocator to emit counters
  auto* trackingAlloc =
      static_cast<runtime::TrackingAllocator*>(GetAllocator());

  GECKO_DEBUG(app::core_example::labels::Memory,
              "Stress-testing memory with 25 random allocations "
              "(64-2048 bytes each)");

  // Allocate random sized blocks with interesting patterns
  for (int i = 0; i < 25; ++i)
  {
    size_t size = gecko::random::Size(64, 4096);
    void* ptr = AllocBytes(size, 16);
    if (ptr)
    {
      allocations.push_back({ptr, size});
      // Write some data to ensure it's valid
      std::memset(ptr, i & 0xFF, size);
    }
    else
    {
      GECKO_WARN(app::core_example::labels::Memory,
                 "Failed to allocate %zu bytes in iteration %d", size, i);
    }

    // Use tracking allocator's live bytes count instead of manual counting
    if (trackingAlloc && (i % 10 == 0))
    {
      GECKO_COUNTER(app::core_example::labels::Memory, "LiveBytes",
                    trackingAlloc->TotalLiveBytes());
    }
  }

  GECKO_DEBUG(app::core_example::labels::Memory,
              "Freeing half of the allocations randomly");

  // Free half randomly - shuffle by swapping random elements
  for (size_t i = 0; i < allocations.size(); ++i)
  {
    size_t swapIndex = gecko::random::Index(allocations.size());
    std::swap(allocations[i], allocations[swapIndex]);
  }

  size_t freedCount = 0;
  for (size_t i = 0; i < allocations.size() / 2; ++i)
  {
    if (allocations[i].first)
    {
      DeallocBytes(allocations[i].first);
      allocations[i].first = nullptr;
      freedCount++;
    }
  }

  GECKO_INFO(app::core_example::labels::Memory, "Freed %zu allocations",
             freedCount);

  // Emit counter after freeing
  if (trackingAlloc)
  {
    GECKO_COUNTER(app::core_example::labels::Memory, "LiveBytesAfterFree",
                  trackingAlloc->TotalLiveBytes());
  }

  GECKO_DEBUG(app::core_example::labels::Memory,
              "Allocating 25 additional blocks with 32-byte alignment");

  // Allocate some more
  for (int i = 0; i < 25; ++i)
  {
    size_t size = gecko::random::Size(64, 4096);
    void* ptr = AllocBytes(size, 32);
    if (ptr)
    {
      allocations.push_back({ptr, size});
      std::memset(ptr, (i + 100) & 0xFF, size);
    }
    else
    {
      GECKO_WARN(app::core_example::labels::Memory,
                 "Failed to allocate %zu bytes in second phase, iteration %d",
                 size, i);
    }
  }

  GECKO_DEBUG(app::core_example::labels::Memory,
              "Cleaning up remaining allocations");

  // Clean up remaining allocations
  size_t cleanupCount = 0;
  for (const auto& alloc : allocations)
  {
    if (alloc.first)
    {
      DeallocBytes(alloc.first);
      cleanupCount++;
    }
  }

  GECKO_INFO(app::core_example::labels::Memory,
             "Cleaned up %zu remaining allocations", cleanupCount);

  // Final counter after cleanup
  if (trackingAlloc)
  {
    GECKO_COUNTER(app::core_example::labels::Memory, "FinalLiveBytes",
                  trackingAlloc->TotalLiveBytes());
  }

  GECKO_INFO(app::core_example::labels::Memory,
             "Memory stress test completed successfully");
}

void PrintMemoryStats(const runtime::TrackingAllocator& tracker)
{
  GECKO_FUNC(app::core_example::labels::Main);

  GECKO_INFO(app::core_example::labels::Main, "=== Memory Statistics ===");
  GECKO_INFO(app::core_example::labels::Main, "Total Live Bytes: %llu",
             tracker.TotalLiveBytes());

  // Print stats for each label we used
  std::vector<Label> labels = {
      app::core_example::labels::Main, app::core_example::labels::Worker,
      app::core_example::labels::Memory, app::core_example::labels::Compute,
      app::core_example::labels::Simulation};

  u64 totalAllocs = 0, totalFrees = 0, totalLive = 0;

  for (auto label : labels)
  {
    runtime::MemLabelStats stats;
    if (tracker.StatsFor(label, stats))
    {
      u64 live = stats.LiveBytes.load();
      u64 allocs = stats.Allocs.load();
      u64 frees = stats.Frees.load();

      totalAllocs += allocs;
      totalFrees += frees;
      totalLive += live;

      if (allocs > 0)
      {
        double percentFreed = (double)frees / allocs * 100.0;
        GECKO_INFO(
            app::core_example::labels::Main,
            "Label '%s' (%llu): Live=%llu bytes, Allocs=%llu, Frees=%llu "
            "(%.1f%% freed)",
            label.Name ? label.Name : "(unnamed)",
            static_cast<unsigned long long>(label.Id), live, allocs, frees,
            percentFreed);
      }
      else
      {
        GECKO_INFO(
            app::core_example::labels::Main,
            "Label '%s' (%llu): Live=%llu bytes, Allocs=%llu, Frees=%llu",
            label.Name ? label.Name : "(unnamed)",
            static_cast<unsigned long long>(label.Id), live, allocs, frees);
      }
    }
  }

  double totalPercentFreed =
      totalAllocs > 0 ? (double)totalFrees / totalAllocs * 100.0 : 0.0;
  GECKO_INFO(app::core_example::labels::Main,
             "Summary: %llu total allocations, %llu freed (%.1f%%), %llu bytes "
             "still allocated",
             totalAllocs, totalFrees, totalPercentFreed, totalLive);

  if (totalLive == 0)
  {
    GECKO_INFO(app::core_example::labels::Main,
               "Perfect! All memory was properly freed.");
  }
  else
  {
    GECKO_WARN(app::core_example::labels::Main,
               "Memory leak detected: %llu bytes still allocated", totalLive);
  }
}

namespace {
struct EventSystemDemoState
{
  std::atomic<u32> onPublishCount {0};
  std::atomic<u32> queuedCount {0};
  EventSubscription onPublishSub {};
  EventSubscription queuedSub {};
};

static void OnTestEventOnPublish(void* user, const EventMeta& meta,
                                 EventView payload)
{
  (void)meta;
  auto* state = static_cast<EventSystemDemoState*>(user);
  const auto* p =
      static_cast<const app::core_example::events::TestEventPayload*>(
          payload.Data());
  state->onPublishCount.fetch_add(1, std::memory_order_relaxed);
  GECKO_INFO(app::core_example::labels::Events, "OnPublish received: value=%u",
             p ? p->value : 0u);
}

static void OnTestEventQueued(void* user, const EventMeta& meta,
                              EventView payload)
{
  (void)meta;
  auto* state = static_cast<EventSystemDemoState*>(user);
  const auto* p =
      static_cast<const app::core_example::events::TestEventPayload*>(
          payload.Data());
  state->queuedCount.fetch_add(1, std::memory_order_relaxed);
  GECKO_INFO(app::core_example::labels::Events,
             "Queued dispatch received: value=%u", p ? p->value : 0u);
}
}  // namespace

static void EventSystemTest()
{
  GECKO_FUNC(app::core_example::labels::Main);

  GECKO_INFO(app::core_example::labels::Main, "Setting up event system demo");

  EventSystemDemoState state {};

  // OnPublish subscriber: gets events immediately during Enqueue()
  state.onPublishSub = SubscribeEvent(
      app::core_example::events::TestEvent, &OnTestEventOnPublish, &state,
      SubscriptionOptions {.delivery = SubscriptionDelivery::OnPublish});

  // Queued subscriber (default): gets events during DispatchQueued()
  state.queuedSub = SubscribeEvent(
      app::core_example::events::TestEvent, &OnTestEventQueued, &state,
      SubscriptionOptions {.delivery = SubscriptionDelivery::Queued});

  // Create emitter using the module's domain.
  // This ensures events are properly scoped to this module.
  const EventEmitter emitter =
      CreateEmitterForModule(app::core_example::labels::App, /*sender=*/0xC0DE);

  // Test 1: PublishEvent (queued) from main thread
  GECKO_INFO(app::core_example::labels::Main,
             "Test 1: PublishEvent from main thread");
  {
    app::core_example::events::TestEventPayload payload {.value = 1};
    PublishEvent(emitter, app::core_example::events::TestEvent, payload);
  }

  // Test 2: PublishEvent from a worker thread (thread-safe enqueue)
  GECKO_INFO(app::core_example::labels::Main,
             "Test 2: PublishEvent from worker thread");
  JobHandle publishJob = SubmitJob(
      [emitter]() {
        app::core_example::events::TestEventPayload payload {.value = 2};
        PublishEvent(emitter, app::core_example::events::TestEvent, payload);
      },
      JobPriority::Normal, app::core_example::labels::Worker);
  WaitForJob(publishJob);

  // Test 3: PublishImmediateEvent - both OnPublish and Queued subscribers
  // should receive it immediately
  GECKO_INFO(app::core_example::labels::Main,
             "Test 3: PublishImmediateEvent (all subscribers notified now)");
  {
    app::core_example::events::TestEventPayload payload {.value = 3};
    PublishImmediateEvent(emitter, app::core_example::events::TestEvent,
                          payload);
  }

  GECKO_INFO(app::core_example::labels::Main,
             "Before DispatchQueued: OnPublish=%u, Queued=%u",
             state.onPublishCount.load(std::memory_order_relaxed),
             state.queuedCount.load(std::memory_order_relaxed));

  // Deliver queued events on the main thread.
  const std::size_t dispatched = DispatchQueuedEvents();
  GECKO_INFO(app::core_example::labels::Main,
             "Event bus dispatched %zu queued events", dispatched);

  GECKO_INFO(app::core_example::labels::Main,
             "After DispatchQueued: OnPublish=%u, Queued=%u",
             state.onPublishCount.load(std::memory_order_relaxed),
             state.queuedCount.load(std::memory_order_relaxed));

  // Expected behavior:
  // - OnPublish: 3 (all 3 events notified immediately)
  // - Queued: 3 (2 from PublishEvent + 1 from PublishImmediateEvent)
}

int main()
{
  // Demonstrate that logging doesn't work before services are initialized
  std::printf("=== Gecko Comprehensive Demo with Logging System ===\n\n");
  std::printf("Attempting to log before services are initialized...\n");

  // This won't produce any output because GetLogger() returns nullptr
  GECKO_INFO(app::core_example::labels::Main,
             "This message won't appear - services not initialized yet!");
  GECKO_WARN(app::core_example::labels::Main, "Neither will this warning!");

  std::printf("(As expected, no log output appeared above)\n\n");

  // Set up our services
  SystemAllocator systemAlloc;
  runtime::TrackingAllocator trackingAlloc(&systemAlloc);
  runtime::RingProfiler ringProfiler(1 << 16);  // 64K events
  runtime::RingLogger ringLogger(1024);  // 1024 log entries in ring buffer
  runtime::ModuleRegistry moduleRegistry;
  runtime::EventBus eventBus;

  // Create job system with 4 worker threads
  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  // Use GECKO_BOOT system for proper service installation and validation
  // Services are in dependency order: Allocator -> JobSystem -> Profiler ->
  // Logger
  GECKO_BOOT((Services {.Allocator = &trackingAlloc,
                        .JobSystem = &jobSystem,
                        .Profiler = &ringProfiler,
                        .Logger = &ringLogger,
                        .Modules = &moduleRegistry,
                        .EventBus = &eventBus}));

  // Configure logging sinks - they auto-unregister when destroyed
  runtime::ConsoleLogSink consoleSink;
  runtime::FileLogSink fileSink("log.txt");

  if (auto* logger = GetLogger())
  {
    fileSink.RegisterWith(logger);
    consoleSink.RegisterWith(logger);
    logger->SetLevel(
        LogLevel::Info);  // Filter out Trace and Debug messages initially
  }

  GECKO_INFO(app::core_example::labels::Main, gecko::VersionFullString());

  (void)InstallModule(runtime::GetModule());
  (void)InstallModule(g_AppModule);

  // Set up trace file sink for profiling data after services are available
  // Sink auto-unregisters when destroyed
  runtime::TraceFileSink traceSink("gecko_trace.json");

  if (!traceSink.IsOpen())
  {
    GECKO_WARN(app::core_example::labels::Main,
               "Failed to open trace profiler sink\n");
  }
  else
  {
    if (auto* profiler = GetProfiler())
      traceSink.RegisterWith(profiler);
  }

  // Now logging works with all sinks configured!
  GECKO_INFO(app::core_example::labels::Main,
             "Services installed and sinks configured successfully!");

  GECKO_DEBUG(
      app::core_example::labels::Main,
      "Logger is now active with immediate logging (this won't appear)");
  GECKO_TRACE(app::core_example::labels::Main,
              "Profiler has capacity for %d events (this won't appear either)",
              (1 << 16));

  GECKO_INFO(
      app::core_example::labels::Main,
      "Log level is set to Info - Debug and Trace messages are filtered out");

  {
    GECKO_FUNC(app::core_example::labels::Main);

    GECKO_INFO(app::core_example::labels::Main,
               "Starting comprehensive memory and profiling demo");

    // Enable more detailed logging for the demo
    GECKO_INFO(app::core_example::labels::Main,
               "Changing log level to Trace to show all messages");
    if (auto* logger = GetLogger())
    {
      logger->SetLevel(LogLevel::Trace);
    }

    // 1. Memory Test
    GECKO_INFO(app::core_example::labels::Main,
               "=== 1. Memory Management Demo ===");
    MemoryStressTest();
    PrintMemoryStats(trackingAlloc);

    // 2. Event System Test
    GECKO_INFO(app::core_example::labels::Main, "=== 2. Event System Demo ===");
    EventSystemTest();

    // 3. Threading Utilities Demo
    GECKO_INFO(app::core_example::labels::Main,
               "=== 3. Threading Utilities Demo ===");
    {
      GECKO_SCOPE_NAMED(app::core_example::labels::Main,
                        "ThreadingUtilitiesDemo");

      u32 threadId = ThisThreadId();
      u32 coreCount = HardwareThreadCount();
      GECKO_INFO(app::core_example::labels::Main,
                 "Current thread ID: %u, Hardware threads: %u", threadId,
                 coreCount);

      // Test precise timing
      u64 start = HighResTimeNs();
      PreciseSleepNs(1000000);  // 1 millisecond
      u64 end = HighResTimeNs();
      u64 elapsed = end - start;
      GECKO_INFO(app::core_example::labels::Main,
                 "PreciseSleepNs(1ms) took %llu ns", elapsed);

      // Test thread yielding
      GECKO_INFO(app::core_example::labels::Main, "Testing thread yield...");
      YieldThread();
      GECKO_INFO(app::core_example::labels::Main, "Thread yield completed");

      // Test different sleep functions
      GECKO_TRACE(app::core_example::labels::Main, "Testing SleepMs(10)...");
      start = HighResTimeNs();
      SleepMs(10);
      end = HighResTimeNs();
      u64 elapsedMs = time::NsToMilliseconds(end - start);
      GECKO_INFO(app::core_example::labels::Main,
                 "SleepMs(10) took approximately %llu ms", elapsedMs);
    }

    // 3. Particle Physics Simulation Demo
    GECKO_INFO(app::core_example::labels::Main,
               "=== 3. Particle Physics Simulation Demo ===");
    const int numWorkers = 3;

    GECKO_INFO(app::core_example::labels::Main,
               "Launching %d parallel physics simulations!", numWorkers);

    std::vector<JobHandle> simulationJobs;

    GECKO_INFO(app::core_example::labels::Main,
               "Submitting %d particle simulation jobs to job system",
               numWorkers);

    {
      GECKO_SCOPE_NAMED(app::core_example::labels::Main,
                        "SubmitSimulationJobs");
      for (int i = 0; i < numWorkers; ++i)
      {
        int particleCount = 800 + i * 400;  // More interesting particle counts
        GECKO_INFO(
            app::core_example::labels::Main,
            "Physics Worker %d: Simulating %d particles in zero-gravity!", i,
            particleCount);

        auto job = [i, particleCount]() { WorkerTask(i, particleCount); };

        auto handle = SubmitJob(job, JobPriority::Normal,
                                app::core_example::labels::Worker);
        simulationJobs.push_back(handle);
      }
    }

    // Wait for all simulation jobs to complete
    GECKO_INFO(app::core_example::labels::Main,
               "Waiting for simulation jobs to complete...");
    {
      GECKO_SCOPE_NAMED(app::core_example::labels::Main,
                        "WaitForSimulationJobs");
      WaitForJobs(simulationJobs.data(),
                  static_cast<u32>(simulationJobs.size()));
    }

    GECKO_INFO(app::core_example::labels::Main,
               "Simulation jobs completed successfully");

    // Print updated memory statistics after simulation
    PrintMemoryStats(trackingAlloc);

    // Emit final counters to profiler
    trackingAlloc.EmitCounters();

    // 4. Logging Demo
    // 4. Extended Simulation (to test crash-safety)\n
    // GECKO_INFO(CoreExampleAppModule::c_main,
    // \"=== 4. Extended Simulation for Crash-Safety Test ===\");\n
    // GECKO_INFO(CoreExampleAppModule::c_main, \"Running extended
    // simulation - interrupt anytime to test crash-safety\");\n    \n    for
    // (int round = 0; round < 10; ++round)
    // {\n      GECKO_INFO(CoreExampleAppModule::c_main, \"Extended
    // simulation round %d/10\", round
    // + 1);\n      \n      std::vector<JobHandle> longJobs;\n      for (int i =
    // 0; i < 4; ++i) {\n        auto job = [i, round]() {\n
    // GECKO_SCOPE_NAMED(CoreExampleAppModule::c_compute,
    // \"ExtendedWork\");\n          for (int work = 0; work < 1000; ++work) {\n
    // // Simulate computational work with profiling
    // GECKO_SCOPE_NAMED(CoreExampleAppModule::c_compute, "WorkUnit");
    // for (volatile int j = 0; j < 10000; ++j) {
    // }
    //
    // if (work % 100 == 0) {
    //     GECKO_PROF_COUNTER(CoreExampleAppModule::c_compute,
    //                        "WorkProgress", work);
    // }
    // GECKO_INFO(CoreExampleAppModule::c_compute,
    //            "Extended job %d in round %d completed", i, round + 1);
    // };
    //
    // auto handle = SubmitJob(job, JobPriority::Normal,
    //                         CoreExampleAppModule::c_compute);
    // longJobs.push_back(handle);
    // }
    //
    // WaitForJobs(longJobs.data(), static_cast<u32>(longJobs.size()));
    // GECKO_INFO(CoreExampleAppModule::c_main,
    //            "Round %d completed", round + 1);
    //
    // // Short delay between rounds
    // SleepMs(200);
    // }
    //
    // GECKO_INFO(CoreExampleAppModule::c_main,
    //            "Extended simulation completed");
    //
    // 5. Logging Demo
    // GECKO_INFO(CoreExampleAppModule::c_main,
    //            "=== 5. Logging System Demo ===");
    GECKO_TRACE(app::core_example::labels::Main,
                "This is a trace message - very detailed info");
    GECKO_DEBUG(app::core_example::labels::Main,
                "This is a debug message - development info");
    GECKO_INFO(app::core_example::labels::Main,
               "This is an info message - general information");
    GECKO_WARN(app::core_example::labels::Main,
               "This is a warning - something might be wrong");
    GECKO_ERROR(
        app::core_example::labels::Main,
        "This is an error - something went wrong (but this is just a test!)");
    // Note: GECKO_FATAL would be for critical errors

    // Job System Example
    GECKO_INFO(app::core_example::labels::Main,
               "Testing Job System with %u worker threads...",
               GetJobSystem()->WorkerThreadCount());
    {
      GECKO_SCOPE_NAMED(app::core_example::labels::Main, "JobSystemDemo");

      // Example with job dependencies - pipeline pattern
      GECKO_INFO(app::core_example::labels::Main,
                 "Testing job dependencies (pipeline pattern)...");

      auto dataPreparationJob = SubmitJob(
          []() {
            GECKO_SCOPE_NAMED(app::core_example::labels::Compute,
                              "PipelineStage1");
            GECKO_INFO(app::core_example::labels::Compute,
                       "Pipeline Stage 1: Loading and validating data...");
            SleepMs(40);
            GECKO_INFO(app::core_example::labels::Compute,
                       "Stage 1: Data preparation complete - "
                       "1000 records processed");
          },
          JobPriority::High, app::core_example::labels::Compute);

      JobHandle stage1Deps[] = {dataPreparationJob};
      auto dataProcessingJob = SubmitJob(
          []() {
            GECKO_SCOPE_NAMED(app::core_example::labels::Compute,
                              "PipelineStage2");
            GECKO_INFO(app::core_example::labels::Compute,
                       "Pipeline Stage 2: Running complex algorithms...");
            SleepMs(50);
            GECKO_INFO(app::core_example::labels::Compute,
                       "Stage 2: Analysis complete - 847 patterns detected");
          },
          stage1Deps, 1, JobPriority::Normal,
          app::core_example::labels::Compute);

      JobHandle stage2Deps[] = {dataProcessingJob};
      auto dataFinalizationJob = SubmitJob(
          []() {
            GECKO_SCOPE_NAMED(app::core_example::labels::Compute,
                              "PipelineStage3");
            GECKO_INFO(app::core_example::labels::Compute,
                       "Pipeline Stage 3: Generating final report...");
            SleepMs(35);
            GECKO_INFO(app::core_example::labels::Compute,
                       "Stage 3: Mission accomplished! Results "
                       "exported to dashboard.");
          },
          stage2Deps, 1, JobPriority::Normal,
          app::core_example::labels::Compute);

      // Process jobs on main thread while waiting to prevent deadlock with 1
      // worker
      while (!IsJobComplete(dataFinalizationJob))
      {
        GetJobSystem()->ProcessJobs(1);
      }
      GECKO_INFO(app::core_example::labels::Main,
                 "Job dependency pipeline completed successfully!");

      // Example of main thread job processing
      GECKO_INFO(app::core_example::labels::Main,
                 "Testing main thread job processing...");
      std::vector<JobHandle> mainThreadJobs;
      for (int i = 0; i < 3; ++i)
      {
        auto job = [i]() {
          GECKO_SCOPE_NAMED(app::core_example::labels::Compute,
                            "MainThreadJob");
          GECKO_INFO(app::core_example::labels::Compute, "Main thread job %d",
                     i);
        };
        auto handle = SubmitJob(job, JobPriority::Low,
                                app::core_example::labels::Compute);
        mainThreadJobs.push_back(handle);
      }

      // Process some jobs on the main thread
      GetJobSystem()->ProcessJobs(2);

      // Wait for any remaining jobs
      WaitForJobs(mainThreadJobs.data(),
                  static_cast<u32>(mainThreadJobs.size()));
      GECKO_INFO(app::core_example::labels::Main,
                 "Main thread job processing complete!");
    }

    // Add a frame mark to separate the main work from cleanup
    if (auto* profiler = GetProfiler())
    {
      ProfEvent frameEvent {.TimestampNs = profiler->NowNs(),
                            .Value = 0,
                            .Name = "EndOfDemo",
                            .EventLabel = app::core_example::labels::Main,
                            .ThreadId = ThisThreadId(),
                            .NameHash = FNV1a("EndOfDemo"),
                            .Kind = ProfEventKind::FrameMark};
      profiler->Emit(frameEvent);
    }
  }

  // Flush any remaining log messages
  GECKO_INFO(app::core_example::labels::Main,
             "Flushing remaining log messages...");

  // The trace sink automatically writes data continuously, so no manual save
  // needed
  GECKO_INFO(app::core_example::labels::Main,
             "Trace data has been continuously written to gecko_trace.json");

  GECKO_INFO(app::core_example::labels::Main, "Shutting down services...");
  if (auto* logger = GetLogger())
  {
    logger->Flush();
  }

  // Unregister sinks before shutting down services
  consoleSink.Unregister();
  fileSink.Unregister();
  traceSink.Unregister();

  // Use GECKO_SHUTDOWN for proper cleanup
  GECKO_SHUTDOWN();

  std::printf("\nDemo completed successfully! Check the log output above.\n");
  std::printf("The immediate logger writes all log messages directly without "
              "buffering.\n");
  std::printf("This ensures immediate output but may be slower than buffered "
              "logging.\n");

  return 0;
}
