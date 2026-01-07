#include <cstdio>
#include <cstring>
#include <vector>

#include "gecko/core/boot.h"
#include "gecko/core/category.h"
#include "gecko/core/events.h"
#include "gecko/core/log.h"
#include "gecko/core/memory.h"
#include "gecko/core/profiler.h"
#include "gecko/core/random.h"
#include "gecko/core/services.h"
#include "gecko/core/thread.h"
#include "gecko/core/time.h"
#include "gecko/core/version.h"
#include "gecko/runtime/console_log_sink.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/file_log_sink.h"
#include "gecko/runtime/ring_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/thread_pool_job_system.h"
#include "gecko/runtime/trace_file_sink.h"
#include "gecko/runtime/tracking_allocator.h"

using namespace gecko;

// Define categories for different subsystems
const auto MAIN_CAT = MakeCategory("Main");
const auto WORKER_CAT = MakeCategory("Worker");
const auto MEMORY_CAT = MakeCategory("Memory");
const auto COMPUTE_CAT = MakeCategory("Compute");
const auto SIMULATION_CAT = MakeCategory("Simulation");
const auto EVENT_CAT = MakeCategory("Event");

// Demo event domain and codes
namespace demo_events {
constexpr u8 DemoDomain = 0x42;
constexpr EventCode DataProcessed = MakeEvent(DemoDomain, 0x0001);
constexpr EventCode TaskComplete = MakeEvent(DemoDomain, 0x0002);

struct DataProcessedPayload {
  u32 itemsProcessed{0};
  float processingTimeMs{0.0f};
};

struct TaskCompletePayload {
  u32 taskId{0};
  bool success{false};
};
} // namespace demo_events

// Simple data structure for our simulation
struct Particle {
  float x, y, z;
  float vx, vy, vz;
  float mass;
};

// Worker function that simulates some computation
void WorkerTask(int workerId, int numParticles) {
  GECKO_PROF_FUNC(WORKER_CAT);

  GECKO_INFO(WORKER_CAT, "Worker %d: Starting simulation with %d particles",
             workerId, numParticles);

  // Allocate particles
  Particle *particles = nullptr;
  {
    GECKO_PROF_SCOPE(MEMORY_CAT, "AllocateParticles");
    particles = AllocArray<Particle>(numParticles, SIMULATION_CAT);
  }
  {
    if (!particles) {
      GECKO_ERROR(WORKER_CAT, "Worker %d: Failed to allocate particles",
                  workerId);
      return;
    }

    GECKO_DEBUG(MEMORY_CAT, "Worker %d: Allocated %zu bytes for %d particles",
                workerId, numParticles * sizeof(Particle), numParticles);

    // Initialize particles
    {
      GECKO_PROF_SCOPE(COMPUTE_CAT, "InitializeParticles");
      // Seed random generator differently per worker for unique particle
      // behaviors
      gecko::SeedRandom(workerId * 12345 + 42);

      for (int i = 0; i < numParticles; ++i) {
        particles[i] = {gecko::RandomFloat(-100.0f, 100.0f),
                        gecko::RandomFloat(-100.0f, 100.0f),
                        gecko::RandomFloat(-100.0f, 100.0f),
                        gecko::RandomFloat(-10.0f, 10.0f),
                        gecko::RandomFloat(-10.0f, 10.0f),
                        gecko::RandomFloat(-10.0f, 10.0f),
                        gecko::RandomFloat(1.0f, 5.0f)};
      }
    }
  }

  {
    GECKO_PROF_SCOPE(COMPUTE_CAT, "PHysicsUpdate");

    // Simulate physics steps (engaging but quick)
    const int numSteps = 50;
    const float deltaTime = 0.016f; // Cache constant
    const float damping = 0.999f;   // Cache constant

    for (int step = 0; step < numSteps; ++step) {
      GECKO_PROF_SCOPE(COMPUTE_CAT, "PhysicsStep");

      // Optimized physics update with better cache usage
      for (int i = 0; i < numParticles; ++i) {
        Particle &p = particles[i]; // Reference for better cache usage

        // Update position
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;
        p.z += p.vz * deltaTime;

        // Apply damping
        p.vx *= damping;
        p.vy *= damping;
        p.vz *= damping;
      }

      // Emit progress counter (reduce frequency to minimize profiling overhead)
      if (step % 20 == 0) {
        GECKO_PROF_COUNTER(WORKER_CAT, "SimulationProgress", step);
      }
    }

    GECKO_TRACE(COMPUTE_CAT, "Worker %d: Completed %d physics steps", workerId,
                numSteps);
    GECKO_INFO(WORKER_CAT,
               "Worker %d: Physics simulation complete! All particles settled.",
               workerId);
  }

  // Clean up
  {
    GECKO_PROF_SCOPE(MEMORY_CAT, "DeallocateParticles");
    DeallocBytes(particles, numParticles * sizeof(Particle), alignof(Particle),
                 SIMULATION_CAT);
    GECKO_DEBUG(MEMORY_CAT, "Worker %d: Freed particle memory", workerId);
  }
}

// Function to perform some memory stress testing
void MemoryStressTest() {
  GECKO_PROF_FUNC(MEMORY_CAT);

  GECKO_INFO(MEMORY_CAT, "Starting memory stress test");

  std::vector<std::pair<void *, size_t>> allocations; // Store pointer and size

  // Get the tracking allocator to emit counters
  auto *trackingAlloc =
      static_cast<runtime::TrackingAllocator *>(GetAllocator());

  GECKO_DEBUG(MEMORY_CAT, "Stress-testing memory with 25 random allocations "
                          "(64-2048 bytes each)");

  // Allocate random sized blocks with interesting patterns
  for (int i = 0; i < 25; ++i) {
    size_t size = gecko::random::Size(64, 4096);
    void *ptr = AllocBytes(size, 16, MEMORY_CAT);
    if (ptr) {
      allocations.push_back({ptr, size});
      // Write some data to ensure it's valid
      std::memset(ptr, i & 0xFF, size);
    } else {
      GECKO_WARN(MEMORY_CAT, "Failed to allocate %zu bytes in iteration %d",
                 size, i);
    }

    // Use tracking allocator's live bytes count instead of manual counting
    if (trackingAlloc && (i % 10 == 0)) {
      GECKO_PROF_COUNTER(MEMORY_CAT, "LiveBytes",
                         trackingAlloc->TotalLiveBytes());
    }
  }

  GECKO_DEBUG(MEMORY_CAT, "Freeing half of the allocations randomly");

  // Free half randomly - shuffle by swapping random elements
  for (size_t i = 0; i < allocations.size(); ++i) {
    size_t swapIndex = gecko::random::Index(allocations.size());
    std::swap(allocations[i], allocations[swapIndex]);
  }

  size_t freedCount = 0;
  for (size_t i = 0; i < allocations.size() / 2; ++i) {
    if (allocations[i].first) {
      DeallocBytes(allocations[i].first, allocations[i].second, 16, MEMORY_CAT);
      allocations[i].first = nullptr;
      freedCount++;
    }
  }

  GECKO_INFO(MEMORY_CAT, "Freed %zu allocations", freedCount);

  // Emit counter after freeing
  if (trackingAlloc) {
    GECKO_PROF_COUNTER(MEMORY_CAT, "LiveBytesAfterFree",
                       trackingAlloc->TotalLiveBytes());
  }

  GECKO_DEBUG(MEMORY_CAT,
              "Allocating 25 additional blocks with 32-byte alignment");

  // Allocate some more
  for (int i = 0; i < 25; ++i) {
    size_t size = gecko::random::Size(64, 4096);
    void *ptr = AllocBytes(size, 32, MEMORY_CAT);
    if (ptr) {
      allocations.push_back({ptr, size});
      std::memset(ptr, (i + 100) & 0xFF, size);
    } else {
      GECKO_WARN(MEMORY_CAT,
                 "Failed to allocate %zu bytes in second phase, iteration %d",
                 size, i);
    }
  }

  GECKO_DEBUG(MEMORY_CAT, "Cleaning up remaining allocations");

  // Clean up remaining allocations
  size_t cleanupCount = 0;
  for (const auto &alloc : allocations) {
    if (alloc.first) {
      DeallocBytes(alloc.first, alloc.second, 16, MEMORY_CAT);
      cleanupCount++;
    }
  }

  GECKO_INFO(MEMORY_CAT, "Cleaned up %zu remaining allocations", cleanupCount);

  // Final counter after cleanup
  if (trackingAlloc) {
    GECKO_PROF_COUNTER(MEMORY_CAT, "FinalLiveBytes",
                       trackingAlloc->TotalLiveBytes());
  }

  GECKO_INFO(MEMORY_CAT, "Memory stress test completed successfully");
}

void EventSystemTest() {
  GECKO_PROF_FUNC(EVENT_CAT);

  GECKO_INFO(EVENT_CAT, "Starting event system test");

  // Create emitter for demo events
  EventEmitter emitter = CreateEmitter(demo_events::DemoDomain);

  // Test 1: Immediate event
  GECKO_INFO(EVENT_CAT, "Test 1: Immediate event publishing");

  struct TestContext {
    int immediateCallCount = 0;
    int queuedCallCount = 0;
    int onPublishCallCount = 0;
  };
  TestContext ctx;

  auto immediateSub = SubscribeEvent(
      demo_events::DataProcessed,
      [](void *user, const EventMeta &meta, EventView payload) {
        auto *context = static_cast<TestContext *>(user);
        auto *data = static_cast<const demo_events::DataProcessedPayload *>(
            payload.Data());
        context->immediateCallCount++;
        GECKO_INFO(EVENT_CAT,
                   "  [Immediate] DataProcessed received: %u items in %.2fms "
                   "(seq=%llu)",
                   data->itemsProcessed, data->processingTimeMs, meta.seq);
      },
      &ctx);

  demo_events::DataProcessedPayload immediatePayload{100, 15.5f};
  PublishImmediateEvent(emitter, demo_events::DataProcessed, immediatePayload);

  GECKO_INFO(EVENT_CAT, "  Immediate callbacks executed: %d",
             ctx.immediateCallCount);

  // Test 2: Queued events
  GECKO_INFO(EVENT_CAT, "Test 2: Queued event publishing");

  auto queuedSub = SubscribeEvent(
      demo_events::TaskComplete,
      [](void *user, const EventMeta &meta, EventView payload) {
        auto *context = static_cast<TestContext *>(user);
        auto *data = static_cast<const demo_events::TaskCompletePayload *>(
            payload.Data());
        context->queuedCallCount++;
        GECKO_INFO(EVENT_CAT,
                   "  [Queued] TaskComplete received: task=%u success=%s "
                   "(seq=%llu)",
                   data->taskId, data->success ? "true" : "false", meta.seq);
      },
      &ctx);

  auto onPublishSub = SubscribeEvent(
      demo_events::TaskComplete,
      [](void *user, const EventMeta &meta, EventView payload) {
        auto *context = static_cast<TestContext *>(user);
        auto *data = static_cast<const demo_events::TaskCompletePayload *>(
            payload.Data());
        context->onPublishCallCount++;
        GECKO_INFO(EVENT_CAT,
                   "  [OnPublish] TaskComplete received ASAP: task=%u success=%s "
                   "(seq=%llu)",
                   data->taskId, data->success ? "true" : "false", meta.seq);
      },
      &ctx,
      SubscriptionOptions{SubscriptionDelivery::OnPublish});

  // Enqueue several events
  for (u32 i = 0; i < 5; ++i) {
    demo_events::TaskCompletePayload payload{i, (i % 2 == 0)};
    PublishEvent(emitter, demo_events::TaskComplete, payload);
  }

  GECKO_INFO(EVENT_CAT,
             "  After PublishEvent() calls, OnPublish callbacks executed: %d",
             ctx.onPublishCallCount);

  GECKO_INFO(EVENT_CAT, "  Enqueued 5 events, dispatching...");
  std::size_t dispatched = DispatchQueuedEvents();
  GECKO_INFO(EVENT_CAT, "  Dispatched %zu events, callbacks executed: %d",
             dispatched, ctx.queuedCallCount);

  // Test 3: Multiple subscribers
  GECKO_INFO(EVENT_CAT, "Test 3: Multiple subscribers");

  int subscriber2Count = 0;
  int subscriber3Count = 0;

  auto sub2 = SubscribeEvent(
      demo_events::DataProcessed,
      [](void *user, const EventMeta &, EventView payload) {
        auto *count = static_cast<int *>(user);
        (*count)++;
        auto *data = static_cast<const demo_events::DataProcessedPayload *>(
            payload.Data());
        GECKO_INFO(EVENT_CAT, "  [Subscriber 2] Received %u items",
                   data->itemsProcessed);
      },
      &subscriber2Count);

  auto sub3 = SubscribeEvent(
      demo_events::DataProcessed,
      [](void *user, const EventMeta &, EventView payload) {
        auto *count = static_cast<int *>(user);
        (*count)++;
        auto *data = static_cast<const demo_events::DataProcessedPayload *>(
            payload.Data());
        GECKO_INFO(EVENT_CAT, "  [Subscriber 3] Received %u items",
                   data->itemsProcessed);
      },
      &subscriber3Count);

  demo_events::DataProcessedPayload multiPayload{250, 22.3f};
  PublishEvent(emitter, demo_events::DataProcessed, multiPayload);
  std::size_t multiDispatched = DispatchQueuedEvents();

  GECKO_INFO(EVENT_CAT,
             "  Dispatched %zu events - Subscriber 1: %d, Subscriber 2: %d, "
             "Subscriber 3: %d",
             multiDispatched, ctx.queuedCallCount, subscriber2Count,
             subscriber3Count);

  // Test 4: Unsubscribe via RAII
  GECKO_INFO(EVENT_CAT, "Test 4: RAII unsubscribe");
  {
    int tempCount = 0;
    auto tempSub = SubscribeEvent(
        demo_events::TaskComplete,
        [](void *user, const EventMeta &, EventView) {
          auto *count = static_cast<int *>(user);
          (*count)++;
          GECKO_INFO(EVENT_CAT, "  [Temp subscriber] Received event");
        },
        &tempCount);

    demo_events::TaskCompletePayload payload{99, true};
    PublishEvent(emitter, demo_events::TaskComplete, payload);
    std::size_t tempDispatched = DispatchQueuedEvents();
    GECKO_INFO(EVENT_CAT, "  Dispatched %zu events, temp subscriber count: %d",
               tempDispatched, tempCount);

    // tempSub goes out of scope here and auto-unsubscribes
  }

  GECKO_INFO(EVENT_CAT, "  Temp subscription destroyed, publishing again...");
  demo_events::TaskCompletePayload payload{100, true};
  PublishEvent(emitter, demo_events::TaskComplete, payload);
  std::size_t dispatchedAfter = DispatchQueuedEvents();
  GECKO_INFO(EVENT_CAT, "  Events dispatched after unsubscribe: %zu",
             dispatchedAfter);

  GECKO_INFO(EVENT_CAT, "Event system test completed successfully!");
}

void PrintMemoryStats(const runtime::TrackingAllocator &tracker) {
  GECKO_PROF_FUNC(MAIN_CAT);

  GECKO_INFO(MAIN_CAT, "=== Memory Statistics ===");
  GECKO_INFO(MAIN_CAT, "Total Live Bytes: %llu", tracker.TotalLiveBytes());

  // Print stats for each category we used
  std::vector<Category> categories = {MAIN_CAT, WORKER_CAT, MEMORY_CAT,
                                      COMPUTE_CAT, SIMULATION_CAT};

  u64 totalAllocs = 0, totalFrees = 0, totalLive = 0;

  for (auto cat : categories) {
    runtime::MemCategoryStats stats;
    if (tracker.StatsFor(cat, stats)) {
      u64 live = stats.LiveBytes.load();
      u64 allocs = stats.Allocs.load();
      u64 frees = stats.Frees.load();

      totalAllocs += allocs;
      totalFrees += frees;
      totalLive += live;

      if (allocs > 0) {
        double percentFreed = (double)frees / allocs * 100.0;
        GECKO_INFO(MAIN_CAT,
                   "Category '%s': Live=%llu bytes, Allocs=%llu, Frees=%llu "
                   "(%.1f%% freed)",
                   stats.Cat.Name ? stats.Cat.Name : "Unknown", live, allocs,
                   frees, percentFreed);
      } else {
        GECKO_INFO(
            MAIN_CAT, "Category '%s': Live=%llu bytes, Allocs=%llu, Frees=%llu",
            stats.Cat.Name ? stats.Cat.Name : "Unknown", live, allocs, frees);
      }
    }
  }

  double totalPercentFreed =
      totalAllocs > 0 ? (double)totalFrees / totalAllocs * 100.0 : 0.0;
  GECKO_INFO(MAIN_CAT,
             "Summary: %llu total allocations, %llu freed (%.1f%%), %llu bytes "
             "still allocated",
             totalAllocs, totalFrees, totalPercentFreed, totalLive);

  if (totalLive == 0) {
    GECKO_INFO(MAIN_CAT, "Perfect! All memory was properly freed.");
  } else {
    GECKO_WARN(MAIN_CAT, "Memory leak detected: %llu bytes still allocated",
               totalLive);
  }
}

int main() {
  // Demonstrate that logging doesn't work before services are initialized
  std::printf("=== Gecko Comprehensive Demo with Logging System ===\n\n");
  std::printf("Attempting to log before services are initialized...\n");

  // This won't produce any output because GetLogger() returns nullptr
  GECKO_INFO(MAIN_CAT,
             "This message won't appear - services not initialized yet!");
  GECKO_WARN(MAIN_CAT, "Neither will this warning!");

  std::printf("(As expected, no log output appeared above)\n\n");

  // Set up our services
  std::printf("Setting up services...\n");
  SystemAllocator systemAlloc;
  runtime::TrackingAllocator trackingAlloc(&systemAlloc);
  runtime::RingProfiler ringProfiler(1 << 16); // 64K events
  runtime::RingLogger ringLogger(1024); // 1024 log entries in ring buffer

  // Create job system with 4 worker threads
  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  // Create event bus
  runtime::EventBus eventBus;

  // Use GECKO_BOOT system for proper service installation and validation
  // Services are in dependency order: Allocator -> JobSystem -> Profiler ->
  // Logger -> EventBus
  GECKO_BOOT((Services{.Allocator = &trackingAlloc,
                       .JobSystem = &jobSystem,
                       .Profiler = &ringProfiler,
                       .Logger = &ringLogger,
                       .EventBus = &eventBus}));

  // Now configure logging sinks after services are installed
  runtime::ConsoleLogSink consoleSink;
  runtime::FileLogSink fileSink("log.txt");

  if (auto *logger = GetLogger()) {
    logger->AddSink(&fileSink);
    logger->AddSink(&consoleSink);
    logger->SetLevel(
        LogLevel::Info); // Filter out Trace and Debug messages initially
  }

  // Set up trace file sink for profiling data after services are available
  runtime::TraceFileSink traceSink("gecko_trace.json");
  if (!traceSink.IsOpen()) {
    GECKO_WARN(MAIN_CAT, "Failed to open trace profiler sink\n");
  } else {
    // We know the profiler is a RingProfiler, so we can safely add the sink
    ringProfiler.AddSink(&traceSink);
  }

  // Now logging works with all sinks configured!
  GECKO_INFO(MAIN_CAT, "Services installed and sinks configured successfully!");
  gecko::LogVersion(MAIN_CAT);
  GECKO_DEBUG(
      MAIN_CAT,
      "Logger is now active with immediate logging (this won't appear)");
  GECKO_TRACE(MAIN_CAT,
              "Profiler has capacity for %d events (this won't appear either)",
              (1 << 16));

  GECKO_INFO(
      MAIN_CAT,
      "Log level is set to Info - Debug and Trace messages are filtered out");

  {
    GECKO_PROF_FUNC(MAIN_CAT);

    GECKO_INFO(MAIN_CAT, "Starting comprehensive memory and profiling demo");

    // Enable more detailed logging for the demo
    GECKO_INFO(MAIN_CAT, "Changing log level to Trace to show all messages");
    if (auto *logger = GetLogger()) {
      logger->SetLevel(LogLevel::Trace);
    }

    // 1. Memory Test
    GECKO_INFO(MAIN_CAT, "=== 1. Memory Management Demo ===");
    MemoryStressTest();
    PrintMemoryStats(trackingAlloc);

    // 2. Event System Test
    GECKO_INFO(MAIN_CAT, "=== 2. Event System Demo ===");
    EventSystemTest();

    // 3. Threading Utilities Demo
    GECKO_INFO(MAIN_CAT, "=== 3. Threading Utilities Demo ===");
    {
      GECKO_PROF_SCOPE(MAIN_CAT, "ThreadingUtilitiesDemo");

      u32 threadId = ThisThreadId();
      u32 coreCount = HardwareThreadCount();
      GECKO_INFO(MAIN_CAT, "Current thread ID: %u, Hardware threads: %u",
                 threadId, coreCount);

      // Test precise timing
      u64 start = HighResTimeNs();
      PreciseSleepNs(1000000); // 1 millisecond
      u64 end = HighResTimeNs();
      u64 elapsed = end - start;
      GECKO_INFO(MAIN_CAT, "PreciseSleepNs(1ms) took %llu ns", elapsed);

      // Test thread yielding
      GECKO_INFO(MAIN_CAT, "Testing thread yield...");
      YieldThread();
      GECKO_INFO(MAIN_CAT, "Thread yield completed");

      // Test different sleep functions
      GECKO_TRACE(MAIN_CAT, "Testing SleepMs(10)...");
      start = HighResTimeNs();
      SleepMs(10);
      end = HighResTimeNs();
      u64 elapsedMs = time::NsToMilliseconds(end - start);
      GECKO_INFO(MAIN_CAT, "SleepMs(10) took approximately %llu ms", elapsedMs);
    }

    // 3. Particle Physics Simulation Demo
    GECKO_INFO(MAIN_CAT, "=== 4. Particle Physics Simulation Demo ===");
    const int numWorkers = 3;

    GECKO_INFO(MAIN_CAT, "Launching %d parallel physics simulations!",
               numWorkers);

    std::vector<JobHandle> simulationJobs;

    GECKO_INFO(MAIN_CAT, "Submitting %d particle simulation jobs to job system",
               numWorkers);

    {
      GECKO_PROF_SCOPE(MAIN_CAT, "SubmitSimulationJobs");
      for (int i = 0; i < numWorkers; ++i) {
        int particleCount = 800 + i * 400; // More interesting particle counts
        GECKO_INFO(
            MAIN_CAT,
            "Physics Worker %d: Simulating %d particles in zero-gravity!", i,
            particleCount);

        auto job = [i, particleCount]() { WorkerTask(i, particleCount); };

        auto handle = SubmitJob(job, JobPriority::Normal, WORKER_CAT);
        simulationJobs.push_back(handle);
      }
    }

    // Wait for all simulation jobs to complete
    GECKO_INFO(MAIN_CAT, "Waiting for simulation jobs to complete...");
    {
      GECKO_PROF_SCOPE(MAIN_CAT, "WaitForSimulationJobs");
      WaitForJobs(simulationJobs.data(),
                  static_cast<u32>(simulationJobs.size()));
    }

    GECKO_INFO(MAIN_CAT, "Simulation jobs completed successfully");

    // Print updated memory statistics after simulation
    PrintMemoryStats(trackingAlloc);

    // Emit final counters to profiler
    trackingAlloc.EmitCounters();

    // 4. Logging Demo
    // 4. Extended Simulation (to test crash-safety)\n    GECKO_INFO(MAIN_CAT,
    // \"=== 4. Extended Simulation for Crash-Safety Test ===\");\n
    // GECKO_INFO(MAIN_CAT, \"Running extended simulation - interrupt anytime to
    // test crash-safety\");\n    \n    for (int round = 0; round < 10; ++round)
    // {\n      GECKO_INFO(MAIN_CAT, \"Extended simulation round %d/10\", round
    // + 1);\n      \n      std::vector<JobHandle> longJobs;\n      for (int i =
    // 0; i < 4; ++i) {\n        auto job = [i, round]() {\n
    // GECKO_PROF_SCOPE(COMPUTE_CAT, \"ExtendedWork\");\n          for (int work
    // = 0; work < 1000; ++work) {\n            // Simulate computational work
    // with profiling\n            GECKO_PROF_SCOPE(COMPUTE_CAT,
    // \"WorkUnit\");\n            for (volatile int j = 0; j < 10000; ++j) {}\n
    // \n            if (work % 100 == 0) {\n GECKO_PROF_COUNTER(COMPUTE_CAT,
    // \"WorkProgress\", work);\n            }\n          }\n
    // GECKO_INFO(COMPUTE_CAT, \"Extended job %d in round %d completed\", i,
    // round + 1);\n        };\n        \n        auto handle = SubmitJob(job,
    // JobPriority::Normal, COMPUTE_CAT);\n        longJobs.push_back(handle);\n
    // }\n      \n      WaitForJobs(longJobs.data(),
    // static_cast<u32>(longJobs.size()));\n      GECKO_INFO(MAIN_CAT, \"Round
    // %d completed\", round + 1);\n      \n      // Short delay between
    // rounds\n      SleepMs(200);\n    }\n    \n    GECKO_INFO(MAIN_CAT,
    // \"Extended simulation completed\");\n\n    // 5. Logging Demo\n
    // GECKO_INFO(MAIN_CAT, \"=== 5. Logging System Demo ===\");
    GECKO_TRACE(MAIN_CAT, "This is a trace message - very detailed info");
    GECKO_DEBUG(MAIN_CAT, "This is a debug message - development info");
    GECKO_INFO(MAIN_CAT, "This is an info message - general information");
    GECKO_WARN(MAIN_CAT, "This is a warning - something might be wrong");
    GECKO_ERROR(
        MAIN_CAT,
        "This is an error - something went wrong (but this is just a test!)");
    // Note: GECKO_FATAL would be for critical errors

    // Job System Example
    GECKO_INFO(MAIN_CAT, "Testing Job System with %u worker threads...",
               GetJobSystem()->WorkerThreadCount());
    {
      GECKO_PROF_SCOPE(MAIN_CAT, "JobSystemDemo");

      std::vector<JobHandle> jobHandles;
      const int numJobs = 6; // Sweet spot for demo

      // Submit some parallel jobs
      GECKO_INFO(MAIN_CAT, "Submitting %d parallel jobs...", numJobs);
      for (int i = 0; i < numJobs; ++i) {
        auto job = [i]() {
          GECKO_INFO(COMPUTE_CAT,
                     "Parallel Worker %d: Processing data chunk...", i);
          // Simulate interesting computational work
          SleepMs(15 + i * 8);
          GECKO_PROF_COUNTER(COMPUTE_CAT, "parallel_jobs_completed", 1);
        };

        auto handle = SubmitJob(job, JobPriority::Normal, COMPUTE_CAT);
        jobHandles.push_back(handle);
      }

      // Wait for all parallel jobs to complete
      GECKO_INFO(MAIN_CAT, "Waiting for all parallel jobs to complete...");
      WaitForJobs(jobHandles.data(), static_cast<u32>(jobHandles.size()));
      GECKO_INFO(MAIN_CAT, "All parallel jobs completed!");

      // Example with job dependencies - pipeline pattern
      GECKO_INFO(MAIN_CAT, "Testing job dependencies (pipeline pattern)...");

      auto dataPreparationJob = SubmitJob(
          []() {
            GECKO_INFO(COMPUTE_CAT,
                       "Pipeline Stage 1: Loading and validating data...");
            SleepMs(40);
            GECKO_INFO(COMPUTE_CAT, "Stage 1: Data preparation complete - "
                                    "1000 records processed");
          },
          JobPriority::High, COMPUTE_CAT);

      JobHandle stage1Deps[] = {dataPreparationJob};
      auto dataProcessingJob = SubmitJob(
          []() {
            GECKO_INFO(COMPUTE_CAT,
                       "Pipeline Stage 2: Running complex algorithms...");
            SleepMs(50);
            GECKO_INFO(COMPUTE_CAT,
                       "Stage 2: Analysis complete - 847 patterns detected");
          },
          stage1Deps, 1, JobPriority::Normal, COMPUTE_CAT);

      JobHandle stage2Deps[] = {dataProcessingJob};
      auto dataFinalizationJob = SubmitJob(
          []() {
            GECKO_INFO(COMPUTE_CAT,
                       "Pipeline Stage 3: Generating final report...");
            SleepMs(35);
            GECKO_INFO(COMPUTE_CAT, "Stage 3: Mission accomplished! Results "
                                    "exported to dashboard.");
          },
          stage2Deps, 1, JobPriority::Normal, COMPUTE_CAT);

      WaitForJob(dataFinalizationJob);
      GECKO_INFO(MAIN_CAT, "Job dependency pipeline completed successfully!");

      // Example of main thread job processing
      GECKO_INFO(MAIN_CAT, "Testing main thread job processing...");
      std::vector<JobHandle> mainThreadJobs;
      for (int i = 0; i < 3; ++i) {
        auto job = [i]() { GECKO_INFO(COMPUTE_CAT, "Main thread job %d", i); };
        auto handle = SubmitJob(job, JobPriority::Low, COMPUTE_CAT);
        mainThreadJobs.push_back(handle);
      }

      // Process some jobs on the main thread
      GetJobSystem()->ProcessJobs(2);

      // Wait for any remaining jobs
      WaitForJobs(mainThreadJobs.data(),
                  static_cast<u32>(mainThreadJobs.size()));
      GECKO_INFO(MAIN_CAT, "Main thread job processing complete!");
    }

    // Add a frame mark to separate the main work from cleanup
    if (auto *profiler = GetProfiler()) {
      ProfEvent frameEvent{ProfEventKind::FrameMark,
                           profiler->NowNs(),
                           ThisThreadId(),
                           MAIN_CAT,
                           FNV1a("EndOfDemo"),
                           "EndOfDemo",
                           0};
      profiler->Emit(frameEvent);
    }
  }

  // Flush any remaining log messages
  GECKO_INFO(MAIN_CAT, "Flushing remaining log messages...");

  // The trace sink automatically writes data continuously, so no manual save
  // needed
  GECKO_INFO(MAIN_CAT,
             "Trace data has been continuously written to gecko_trace.json");

  GECKO_INFO(MAIN_CAT, "Shutting down services...");
  if (auto *logger = GetLogger()) {
    logger->Flush();
  }

  // Clean up the trace sink (destructor handles cleanup)
  // traceSink.Shutdown(); // Not needed - destructor handles cleanup

  // Use GECKO_SHUTDOWN for proper cleanup
  GECKO_SHUTDOWN();

  std::printf("\nDemo completed successfully! Check the log output above.\n");
  std::printf("The immediate logger writes all log messages directly without "
              "buffering.\n");
  std::printf("This ensures immediate output but may be slower than buffered "
              "logging.\n");

  return 0;
}
