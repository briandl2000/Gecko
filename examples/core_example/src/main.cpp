#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

#include "gecko/core/core.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/trace_writer.h"

using namespace gecko;

// Define some categories for different subsystems
constexpr auto MAIN_CAT = MakeCategory("Main");
constexpr auto WORKER_CAT = MakeCategory("Worker");
constexpr auto MEMORY_CAT = MakeCategory("Memory");
constexpr auto COMPUTE_CAT = MakeCategory("Compute");

// Simple allocator implementation for demonstration
class SimpleAllocator : public IAllocator 
{
public:
    void* alloc(u64 size, u32 alignment, Category category) noexcept override 
    {
        GECKO_PROF_ZONE(MEMORY_CAT, "SimpleAllocator::alloc");
        
        // Align the size to the requested alignment
        size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);
        void* ptr = _aligned_malloc(alignedSize, alignment);
        
        if (ptr) {
            m_TotalAllocated.fetch_add(alignedSize, std::memory_order_relaxed);
            m_AllocationCount.fetch_add(1, std::memory_order_relaxed);
            
            GECKO_PROF_COUNTER(MEMORY_CAT, "TotalAllocated", m_TotalAllocated.load());
            GECKO_PROF_COUNTER(MEMORY_CAT, "AllocationCount", m_AllocationCount.load());
        }
        
        return ptr;
    }

    void free(void* ptr, u64 size, u32 alignment, Category category) noexcept override 
    {
        GECKO_PROF_ZONE(MEMORY_CAT, "SimpleAllocator::free");
        
        
        if (ptr) {
            _aligned_free(ptr);
    
            size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);
            m_TotalAllocated.fetch_sub(alignedSize, std::memory_order_relaxed);
            m_AllocationCount.fetch_sub(1, std::memory_order_relaxed);
            
            GECKO_PROF_COUNTER(MEMORY_CAT, "TotalAllocated", m_TotalAllocated.load());
            GECKO_PROF_COUNTER(MEMORY_CAT, "AllocationCount", m_AllocationCount.load());
        }
    }

    u64 GetTotalAllocated() const noexcept { return m_TotalAllocated.load(); }
    u64 GetAllocationCount() const noexcept { return m_AllocationCount.load(); }

private:
    std::atomic<u64> m_TotalAllocated { 0 };
    std::atomic<u64> m_AllocationCount { 0 };
};

// Worker function that performs some computation
void WorkerFunction(int workerId, int iterations)
{
    GECKO_PROF_FUNC(WORKER_CAT);
    
    std::mt19937 rng(workerId);
    std::uniform_real_distribution<f32> dist(0.0f, 1000.0f);
    
    // Allocate some memory using Gecko's allocator
    constexpr u64 arraySize = 1000;
    auto* data = AllocArray<f32>(arraySize, COMPUTE_CAT);
    
    if (!data) {
        std::cerr << "Failed to allocate memory for worker " << workerId << std::endl;
        return;
    }
    
    f64 sum = 0.0;
    
    for (int i = 0; i < iterations; ++i) {
        GECKO_PROF_ZONE(COMPUTE_CAT, "ComputeIteration");
        
        // Fill array with random values
        for (u64 j = 0; j < arraySize; ++j) {
            data[j] = dist(rng);
        }
        
        // Perform some computation
        f64 iterationSum = 0.0;
        for (u64 j = 0; j < arraySize; ++j) {
            iterationSum += data[j] * data[j];
        }
        
        sum += iterationSum;
        
        GECKO_PROF_COUNTER(WORKER_CAT, "IterationSum", static_cast<u64>(iterationSum));
        
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "Worker " << workerId << " completed with sum: " << sum << std::endl;
    
    // Clean up memory
    DeallocBytes(data, sizeof(f32) * arraySize, alignof(f32), COMPUTE_CAT);
}

// Function to demonstrate memory operations
void MemoryDemo()
{
    GECKO_PROF_FUNC(MEMORY_CAT);
    
    std::cout << "=== Memory Demo ===" << std::endl;
    
    // Allocate various sized blocks
    std::vector<void*> allocations;
    std::vector<u64> sizes;
    
    for (int i = 0; i < 10; ++i) {
        u64 size = (i + 1) * 64;
        void* ptr = AllocBytes(size, 16, MEMORY_CAT);
        
        if (ptr) {
            allocations.push_back(ptr);
            sizes.push_back(size);
            std::cout << "Allocated " << size << " bytes at " << ptr << std::endl;
        }
    }
    
    // Free all allocations
    for (size_t i = 0; i < allocations.size(); ++i) {
        DeallocBytes(allocations[i], sizes[i], 16, MEMORY_CAT);
        std::cout << "Freed " << sizes[i] << " bytes at " << allocations[i] << std::endl;
    }
    
    std::cout << "Memory demo completed!" << std::endl;
}

// Function to demonstrate profiling features
void ProfilingDemo()
{
    GECKO_PROF_FUNC(MAIN_CAT);
    
    std::cout << "=== Profiling Demo ===" << std::endl;
    
    for (int i = 0; i < 5; ++i) {
        GECKO_PROF_ZONE(MAIN_CAT, "ProfilingLoop");
        
        GECKO_PROF_COUNTER(MAIN_CAT, "LoopIteration", i);
        
        // Simulate some work with nested profiling
        {
            GECKO_PROF_ZONE(COMPUTE_CAT, "NestedWork");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        GECKO_PROF_FRAME(MAIN_CAT, "DemoFrame");
    }
    
    std::cout << "Profiling demo completed!" << std::endl;
}

int main()
{
    // Create services
    auto allocator = CreateUnique<SimpleAllocator>();
    auto profiler = CreateUnique<runtime::RingProfiler>();
    
    GECKO_BOOT(::gecko::Services( {
        .Allocator = allocator.get(),
        .Profiler = profiler.get()
    }));

    {
      auto traceWriter = CreateUnique<runtime::TraceWriter>();

      std::cout << "Gecko Core Services Example" << std::endl;
      std::cout << "===========================" << std::endl;
      

      // Setup trace output
      if (!traceWriter->Open("gecko_trace.json")) {
          std::cerr << "Warning: Could not open trace file for writing" << std::endl;
      }
      
      
      std::cout << "Services installed successfully!" << std::endl;
      
      {
          GECKO_PROF_FUNC(MAIN_CAT);
          
          // Demonstrate memory operations
          MemoryDemo();
          
          std::cout << std::endl;
          
          // Demonstrate profiling
          ProfilingDemo();
          
          std::cout << std::endl;
          
          // Demonstrate multithreading
          std::cout << "=== Multithreading Demo ===" << std::endl;
          
          const int numWorkers = 4;
          const int iterationsPerWorker = 50;
          
          std::vector<std::thread> workers;
          workers.reserve(numWorkers);
          
          auto startTime = std::chrono::high_resolution_clock::now();
          
          // Launch worker threads
          for (int i = 0; i < numWorkers; ++i) {
              workers.emplace_back(WorkerFunction, i, iterationsPerWorker);
          }
          
          // Wait for all workers to complete
          for (auto& worker : workers) {
              worker.join();
          }
          
          auto endTime = std::chrono::high_resolution_clock::now();
          auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
          
          std::cout << "All workers completed in " << duration.count() << "ms" << std::endl;
          
          // Display memory statistics
          std::cout << std::endl;
          std::cout << "=== Memory Statistics ===" << std::endl;
          std::cout << "Total allocated: " << allocator->GetTotalAllocated() << " bytes" << std::endl;
          std::cout << "Active allocations: " << allocator->GetAllocationCount() << std::endl;
          
          // Process and write profiling events
          std::cout << std::endl;
          std::cout << "=== Processing Profiling Events ===" << std::endl;
          
          ProfEvent event;
          int eventCount = 0;
          while (profiler->TryPop(event)) {
              traceWriter->Write(event);
              eventCount++;
          }
          
          std::cout << "Processed " << eventCount << " profiling events" << std::endl;
      }
      
      traceWriter->Close();
      
      

      std::cout << std::endl;
      std::cout << "Example completed successfully!" << std::endl;
      std::cout << "Check 'gecko_trace.json' for profiling data" << std::endl;
    }

    // Final memory allocation summary before shutdown
    std::cout << std::endl;
    std::cout << "=== Final Memory Summary ===" << std::endl;
    std::cout << "Total memory still allocated: " << allocator->GetTotalAllocated() << " bytes" << std::endl;
    std::cout << "Active allocation count: " << allocator->GetAllocationCount() << std::endl;
    
    if (allocator->GetAllocationCount() == 0) {
        std::cout << "✓ All memory properly deallocated!" << std::endl;
    } else {
        std::cout << "⚠ Warning: " << allocator->GetAllocationCount() << " allocations still active" << std::endl;
    }

    GECKO_SHUTDOWN();


    return 0;
}
