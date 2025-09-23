#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <cstdio>

#include "gecko/core/core.h"
#include "gecko/core/services.h"
#include "gecko/core/memory.h"
#include "gecko/core/profiler.h"
#include "gecko/core/boot.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/trace_writer.h"
#include "gecko/runtime/tracking_allocator.h"

using namespace gecko;

// Define categories for different subsystems
constexpr auto MAIN_CAT = MakeCategory("Main");
constexpr auto WORKER_CAT = MakeCategory("Worker");
constexpr auto MEMORY_CAT = MakeCategory("Memory");
constexpr auto COMPUTE_CAT = MakeCategory("Compute");
constexpr auto SIMULATION_CAT = MakeCategory("Simulation");

// Simple data structure for our simulation
struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float mass;
};

// Worker function that simulates some computation
void WorkerTask(int workerId, int numParticles) {
    GECKO_PROF_FUNC(WORKER_CAT);
    
    std::printf("Worker %d: Starting simulation with %d particles\n", workerId, numParticles);
    
    // Allocate particles
    {
        GECKO_PROF_SCOPE(MEMORY_CAT, "AllocateParticles");
        auto* particles = AllocArray<Particle>(numParticles, SIMULATION_CAT);
        
        if (!particles) {
            std::printf("Worker %d: Failed to allocate particles\n", workerId);
            return;
        }
        
        // Initialize particles
        {
            GECKO_PROF_SCOPE(COMPUTE_CAT, "InitializeParticles");
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> pos(-100.0f, 100.0f);
            std::uniform_real_distribution<float> vel(-10.0f, 10.0f);
            std::uniform_real_distribution<float> mass(1.0f, 5.0f);
            
            for (int i = 0; i < numParticles; ++i) {
                particles[i] = {
                    pos(gen), pos(gen), pos(gen),
                    vel(gen), vel(gen), vel(gen),
                    mass(gen)
                };
            }
        }
        
        // Simulate physics steps
        const int numSteps = 100;
        for (int step = 0; step < numSteps; ++step) {
            GECKO_PROF_SCOPE(COMPUTE_CAT, "PhysicsStep");
            
            // Simple physics update
            for (int i = 0; i < numParticles; ++i) {
                particles[i].x += particles[i].vx * 0.016f; // 16ms timestep
                particles[i].y += particles[i].vy * 0.016f;
                particles[i].z += particles[i].vz * 0.016f;
                
                // Apply some damping
                particles[i].vx *= 0.999f;
                particles[i].vy *= 0.999f;
                particles[i].vz *= 0.999f;
            }
            
            // Emit progress counter
            if (step % 10 == 0) {
                GECKO_PROF_COUNTER(WORKER_CAT, "SimulationProgress", step);
            }
        }
        
        std::printf("Worker %d: Completed simulation\n", workerId);
        
        // Clean up
        {
            GECKO_PROF_SCOPE(MEMORY_CAT, "DeallocateParticles");
            DeallocBytes(particles, numParticles * sizeof(Particle), alignof(Particle), SIMULATION_CAT);
        }
    }
}

// Function to perform some memory stress testing
void MemoryStressTest() {
    GECKO_PROF_FUNC(MEMORY_CAT);
    
    std::vector<std::pair<void*, size_t>> allocations; // Store pointer and size
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> sizeDist(64, 4096);
    
    // Get the tracking allocator to emit counters
    auto* trackingAlloc = static_cast<runtime::TrackingAllocator*>(GetAllocator());
    
    // Allocate random sized blocks
    for (int i = 0; i < 50; ++i) {
        size_t size = sizeDist(gen);
        void* ptr = AllocBytes(size, 16, MEMORY_CAT);
        if (ptr) {
            allocations.push_back({ptr, size});
            // Write some data to ensure it's valid
            std::memset(ptr, i & 0xFF, size);
        }
        
        // Use tracking allocator's live bytes count instead of manual counting
        if (trackingAlloc && (i % 10 == 0)) {
            GECKO_PROF_COUNTER(MEMORY_CAT, "LiveBytes", trackingAlloc->TotalLiveBytes());
        }
    }
    
    // Free half randomly
    std::shuffle(allocations.begin(), allocations.end(), gen);
    for (size_t i = 0; i < allocations.size() / 2; ++i) {
        if (allocations[i].first) {
            DeallocBytes(allocations[i].first, allocations[i].second, 16, MEMORY_CAT);
            allocations[i].first = nullptr;
        }
    }
    
    // Emit counter after freeing
    if (trackingAlloc) {
        GECKO_PROF_COUNTER(MEMORY_CAT, "LiveBytesAfterFree", trackingAlloc->TotalLiveBytes());
    }
    
    // Allocate some more
    for (int i = 0; i < 25; ++i) {
        size_t size = sizeDist(gen);
        void* ptr = AllocBytes(size, 32, MEMORY_CAT);
        if (ptr) {
            allocations.push_back({ptr, size});
            std::memset(ptr, (i + 100) & 0xFF, size);
        }
    }
    
    // Clean up remaining allocations
    for (const auto& alloc : allocations) {
        if (alloc.first) {
            DeallocBytes(alloc.first, alloc.second, 16, MEMORY_CAT);
        }
    }
    
    // Final counter after cleanup
    if (trackingAlloc) {
        GECKO_PROF_COUNTER(MEMORY_CAT, "FinalLiveBytes", trackingAlloc->TotalLiveBytes());
    }
}

void PrintMemoryStats(const runtime::TrackingAllocator& tracker) {
    GECKO_PROF_FUNC(MAIN_CAT);
    
    std::printf("\n=== Memory Statistics ===\n");
    std::printf("Total Live Bytes: %llu\n", tracker.TotalLiveBytes());
    
    // Print stats for each category we used
    std::vector<Category> categories = { MAIN_CAT, WORKER_CAT, MEMORY_CAT, COMPUTE_CAT, SIMULATION_CAT };
    
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
            
            std::printf("Category '%s': Live=%llu bytes, Allocs=%llu, Frees=%llu",
                       stats.Category.Name ? stats.Category.Name : "Unknown",
                       live, allocs, frees);
            
            if (allocs > 0) {
                std::printf(" (%.1f%% freed)", (double)frees / allocs * 100.0);
            }
            std::printf("\n");
        }
    }
    
    std::printf("Summary: %llu total allocations, %llu freed (%.1f%%), %llu bytes still allocated\n",
               totalAllocs, totalFrees, 
               totalAllocs > 0 ? (double)totalFrees / totalAllocs * 100.0 : 0.0,
               totalLive);
}

void SaveTraceData(runtime::RingProfiler& profiler) {
    GECKO_PROF_FUNC(MAIN_CAT);
    
    std::printf("\nSaving trace data...\n");
    
    runtime::TraceWriter writer;
    if (!writer.Open("gecko_trace.json")) {
        std::printf("Failed to open trace file\n");
        return;
    }
    
    // Extract events from ring buffer
    ProfEvent event;
    int eventCount = 0;
    while (profiler.TryPop(event)) {
        writer.Write(event);
        eventCount++;
    }
    
    writer.Close();
    std::printf("Saved %d events to gecko_trace.json\n", eventCount);
    std::printf("You can view this trace in Chrome by opening chrome://tracing/ and loading the file\n");
}

int main() {
    std::printf("=== Gecko Tracking Allocator & Ring Profiler Demo ===\n\n");
    
    // Set up our services
    SystemAllocator systemAlloc;
    runtime::TrackingAllocator trackingAlloc(&systemAlloc);
    runtime::RingProfiler ringProfiler(1 << 16); // 64K events
    
    // Use GECKO_BOOT system for proper service installation and validation
    GECKO_BOOT((Services{ 
        .Allocator = &trackingAlloc, 
        .Profiler = &ringProfiler 
    }));
    
    std::printf("Services installed and validated successfully\n");
    
    {
        GECKO_PROF_FUNC(MAIN_CAT);
        
        // Perform memory stress test
        {
            GECKO_PROF_SCOPE(MAIN_CAT, "MemoryStressTest");
            MemoryStressTest();
        }
        
        // Launch worker threads for simulation
        std::vector<std::thread> workers;
        const int numWorkers = 3;
        
        {
            GECKO_PROF_SCOPE(MAIN_CAT, "LaunchWorkers");
            for (int i = 0; i < numWorkers; ++i) {
                workers.emplace_back(WorkerTask, i, 1000 + i * 500);
            }
        }
        
        // Wait for workers to complete
        {
            GECKO_PROF_SCOPE(MAIN_CAT, "WaitForWorkers");
            for (auto& worker : workers) {
                worker.join();
            }
        }
        
        // Print memory statistics
        PrintMemoryStats(trackingAlloc);
        
        // Emit final counters to profiler
        trackingAlloc.EmitCounters();
        
        // Add a frame mark to separate the main work from cleanup
        if (auto* profiler = GetProfiler()) {
            ProfEvent frameEvent{
                ProfEventKind::FrameMark,
                profiler->NowNs(),
                ThisThreadId(),
                MAIN_CAT,
                FNV1a("EndOfDemo"),
                "EndOfDemo",
                0
            };
            profiler->Emit(frameEvent);
        }
    }
    
    // Save trace data
    SaveTraceData(ringProfiler);
    
    // Use GECKO_SHUTDOWN for proper cleanup
    GECKO_SHUTDOWN();
    std::printf("\nDemo completed successfully!\n");
    
    return 0;
}
