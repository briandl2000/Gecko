# Gecko Framework - Coding Standards & API Guide

## Table of Contents
- [Coding Standards](#coding-standards)
- [API Usage Guide](#api-usage-guide)
- [Best Practices](#best-practices)
- [Common Patterns](#common-patterns)

## Coding Standards

### File Organization

#### Header Files (.h)
```cpp
#pragma once

// System includes first (alphabetical order)
#include <atomic>
#include <cstdint>
#include <memory>

// Project includes second (alphabetical order)
#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/types.h"

namespace gecko {
  // Content here
}
```

#### Source Files (.cpp)
```cpp
#include "header_file.h"  // Corresponding header first

// System includes (alphabetical order)
#include <atomic>
#include <cstdio>
#include <memory>

// Project includes (alphabetical order)
#include "gecko/core/assert.h"
#include "gecko/core/core.h"

// Local includes last
#include "categories.h"

namespace gecko {
  // Implementation here
}
```

### Naming Conventions

- **Types**: PascalCase (`LogLevel`, `IAllocator`, `PlatformContext`)
- **Functions**: PascalCase (`GetAllocator()`, `InstallServices()`)
- **Variables**: camelCase (`timeNs`, `threadId`)
- **Member Variables**: m_ prefix (`m_File`, `m_ThreadSafe`, `m_Level`)
- **Static/Global Variables**: g_ or s_ prefix (`g_Allocator`, `s_SystemAllocator`)
- **Constants**: PascalCase or ALL_CAPS (`LogLevel::Info`, `GECKO_API`)
- **Namespaces**: lowercase (`gecko`, `runtime`, `platform`)

### Formatting Standards

#### Spacing and Braces
```cpp
// Function definitions
void Function(int param) noexcept
{
  // Body here
}

// Control structures
if (condition)
{
  // Body
}
else
{
  // Body
}

// Single line conditions (only for simple cases)
if (!ptr) return;
```

#### Include Organization
- Blank line after `#pragma once`
- Blank line between system and project includes
- Blank line between project and local includes
- Alphabetical order within each group

#### Assertions
Strategic assertions should be placed at:
- Function entry points for parameter validation
- Before potentially dangerous operations
- Resource allocation/deallocation points

```cpp
void Function(const char* str, size_t size)
{
  GECKO_ASSERT(str && "String parameter cannot be null");
  GECKO_ASSERT(size > 0 && "Size must be greater than zero");
  // Implementation
}
```

### Platform-Specific Code
```cpp
#if defined(_WIN32)
  // Windows-specific code
  #include <corecrt_share.h>
  m_File = _fsopen(path, "wb", _SH_DENYNO);
#else
  // POSIX code
  m_File = std::fopen(path, "wb");
#endif
```

## API Usage Guide

### Service Management

The Gecko framework uses a centralized service system for core functionality:

#### Basic Setup
```cpp
#include "gecko/core/core.h"

int main()
{
  // Method 1: Using GECKO_BOOT macro
  gecko::Services services{};
  services.Allocator = &myAllocator;  // Optional: custom allocator
  services.Logger = &myLogger;        // Optional: custom logger
  services.Profiler = &myProfiler;    // Optional: custom profiler
  
  GECKO_BOOT(services);  // Installs and validates services
  
  // Your application code here
  
  GECKO_SHUTDOWN();  // Clean shutdown
  return 0;
}

// Method 2: Manual service management
int main()
{
  gecko::Services services{};
  // Configure services...
  
  if (!gecko::InstallServices(services))
  {
    // Handle installation failure
    return -1;
  }
  
  if (!gecko::ValidateServices(true))  // true = fatal on failure
  {
    // Handle validation failure
    return -1;
  }
  
  // Application code...
  
  gecko::UninstallServices();
  return 0;
}
```

### Memory Management

#### Basic Allocation
```cpp
#include "gecko/core/memory.h"

// Basic byte allocation
auto category = gecko::MakeCategory("my_system");
void* ptr = gecko::AllocBytes(1024, 16, category);  // 1024 bytes, 16-byte aligned
gecko::DeallocBytes(ptr, 1024, 16, category);

// Type-safe array allocation
int* array = gecko::AllocArray<int>(100, category);  // 100 integers
// Use array...
gecko::DeallocBytes(array, sizeof(int) * 100, alignof(int), category);
```

#### Custom Allocators
```cpp
class MyAllocator : public gecko::IAllocator
{
public:
  virtual void* Alloc(gecko::u64 size, gecko::u32 alignment, gecko::Category category) noexcept override
  {
    GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
    GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");
    
    // Your allocation logic here
    return allocate_memory(size, alignment);
  }
  
  virtual void Free(void* ptr, gecko::u64 size, gecko::u32 alignment, gecko::Category category) noexcept override
  {
    if (!ptr) return;
    // Your deallocation logic here
    free_memory(ptr);
  }
  
  virtual bool Init() noexcept override { return true; }
  virtual void Shutdown() noexcept override { /* cleanup */ }
};
```

### Logging System

#### Basic Logging
```cpp
#include "gecko/core/log.h"

auto category = gecko::MakeCategory("my_system");

// Using convenience macros
GECKO_TRACE(category, "Detailed trace information: %d", value);
GECKO_DEBUG(category, "Debug info: %s", debugString);
GECKO_INFO(category, "General information");
GECKO_WARN(category, "Warning message: %s", warning);
GECKO_ERROR(category, "Error occurred: %d", errorCode);
GECKO_FATAL(category, "Fatal error: %s", fatalMessage);

// Direct logger access
auto* logger = gecko::GetLogger();
logger->Log(gecko::LogLevel::Info, category, "Message: %s", text);
```

#### Custom Log Sinks
```cpp
class MyLogSink : public gecko::ILogSink
{
public:
  virtual void Write(const gecko::LogMessage& message) noexcept override
  {
    GECKO_ASSERT(message.Text && "Log message text cannot be null");
    
    // Format: [LEVEL][CATEGORY][THREAD] MESSAGE
    printf("[%s][%s][t%u] %s\n",
           gecko::LevelName(message.Level),
           message.Category.Name ? message.Category.Name : "unknown",
           message.ThreadId,
           message.Text);
  }
};

// Usage
MyLogSink sink;
gecko::GetLogger()->AddSink(&sink);
```

#### Advanced Loggers
```cpp
// Immediate logger (thread-safe, direct output)
gecko::runtime::ImmediateLogger logger;
logger.SetThreadSafe(true);
logger.SetLevel(gecko::LogLevel::Debug);

// Ring buffer logger (high-performance, background thread)
gecko::runtime::RingLogger ringLogger(8192);  // 8K ring buffer
ringLogger.SetLevel(gecko::LogLevel::Trace);
```

### Profiling System

#### Basic Profiling
```cpp
#include "gecko/core/profiler.h"

auto category = gecko::MakeCategory("my_system");

// Function profiling
void MyFunction()
{
  GECKO_PROF_FUNC(category);  // Profiles entire function
  // Function body...
}

// Scope profiling
void ProcessData()
{
  {
    GECKO_PROF_SCOPE(category, "data_loading");
    // Load data...
  }
  
  {
    GECKO_PROF_SCOPE(category, "data_processing");
    // Process data...
  }
}

// Counters and frame markers
GECKO_PROF_COUNTER(category, "active_objects", objectCount);
GECKO_PROF_FRAME(category, "main_loop");
```

#### Custom Profilers
```cpp
class MyProfiler : public gecko::IProfiler
{
public:
  virtual void Emit(const gecko::ProfEvent& event) noexcept override
  {
    switch (event.Kind)
    {
      case gecko::ProfEventKind::ZoneBegin:
        // Handle zone start
        handleZoneBegin(event);
        break;
      case gecko::ProfEventKind::ZoneEnd:
        // Handle zone end
        handleZoneEnd(event);
        break;
      case gecko::ProfEventKind::Counter:
        // Handle counter
        handleCounter(event);
        break;
      case gecko::ProfEventKind::FrameMark:
        // Handle frame marker
        handleFrameMark(event);
        break;
    }
  }
  
  virtual gecko::u64 NowNs() const noexcept override
  {
    return getCurrentTimeNanoseconds();
  }
  
  virtual bool Init() noexcept override { return true; }
  virtual void Shutdown() noexcept override { /* cleanup */ }
};
```

### Categories System

Categories are used for organizing and filtering logs/profiling data:

```cpp
// Create categories
auto mySystemCategory = gecko::MakeCategory("my_system");
auto networkCategory = gecko::MakeCategory("network");
auto renderCategory = gecko::MakeCategory("renderer");

// Categories are compile-time constants when possible
namespace categories {
  inline constexpr auto MySystem = gecko::MakeCategory("my_system");
  inline constexpr auto Network = gecko::MakeCategory("network");
  inline constexpr auto Renderer = gecko::MakeCategory("renderer");
}
```

### Runtime Components

#### Tracking Allocator
```cpp
gecko::runtime::TrackingAllocator tracker;
tracker.SetUpstream(&myBaseAllocator);  // Base allocator to wrap
tracker.SetProfiler(gecko::GetProfiler());  // For counter emission

// Use as normal allocator - automatically tracks statistics
// Get statistics
gecko::runtime::MemCategoryStats stats;
if (tracker.StatsFor(myCategory, stats))
{
  printf("Live bytes: %llu\n", stats.LiveBytes.load());
  printf("Total allocs: %llu\n", stats.Allocs.load());
  printf("Total frees: %llu\n", stats.Frees.load());
}
```

#### Trace Writer
```cpp
gecko::runtime::TraceWriter traceWriter;
if (traceWriter.Open("trace.json"))
{
  // Connect to ring profiler
  gecko::runtime::RingProfiler profiler(8192);
  
  // In your main loop:
  gecko::ProfEvent event;
  while (profiler.TryPop(event))
  {
    traceWriter.Write(event);
  }
  
  traceWriter.Close();
}
```

### Smart Pointers

Gecko provides standard smart pointer aliases:

```cpp
#include "gecko/core/ptr.h"

// Creating smart pointers
auto uniquePtr = gecko::CreateUnique<MyClass>(constructor_args);
auto sharedPtr = gecko::CreateShared<MyClass>(constructor_args);

// From raw pointers (use with caution)
auto uniqueFromRaw = gecko::CreateUniqueFromRaw(rawPtr);
auto sharedFromRaw = gecko::CreateSharedFromRaw(rawPtr);

// Weak pointers
auto weakPtr = gecko::CreateWeakFromShared(sharedPtr);
```

## Best Practices

### Error Handling
- Use assertions liberally for development-time checks
- Validate parameters at function entry points
- Check resource allocation success
- Handle service initialization failures gracefully

### Performance
- Use ring buffers for high-frequency operations (logging, profiling)
- Prefer immediate mode for low-frequency, important operations
- Category-based filtering for conditional operations
- Memory alignment validation in debug builds

### Thread Safety
- Services are thread-safe by design (using atomics)
- Ring buffers are lock-free for producer threads
- Immediate logger can be made thread-safe via `SetThreadSafe(true)`
- Always validate thread safety requirements for custom implementations

### Resource Management
- Always pair `Init()` with `Shutdown()` calls
- Use RAII patterns where possible
- Clean up services on application exit
- Validate service states before use

## Common Patterns

### Service Implementation Template
```cpp
class MyService : public gecko::IServiceInterface
{
public:
  virtual bool Init() noexcept override
  {
    GECKO_ASSERT(!m_Initialized && "Service already initialized");
    // Initialization logic
    m_Initialized = true;
    return true;
  }
  
  virtual void Shutdown() noexcept override
  {
    if (!m_Initialized) return;
    // Cleanup logic
    m_Initialized = false;
  }
  
private:
  bool m_Initialized { false };
};
```

### Category Declaration Pattern
```cpp
// In categories.h
namespace my_project::categories {
  inline constexpr auto System = gecko::MakeCategory("system");
  inline constexpr auto Network = gecko::MakeCategory("network");
  inline constexpr auto Renderer = gecko::MakeCategory("renderer");
}
```

### Profiled Function Pattern
```cpp
void ExpensiveFunction()
{
  GECKO_PROF_FUNC(categories::System);
  
  // Function implementation
  {
    GECKO_PROF_SCOPE(categories::System, "initialization");
    // Initialization phase
  }
  
  {
    GECKO_PROF_SCOPE(categories::System, "processing");
    // Processing phase
  }
  
  GECKO_PROF_COUNTER(categories::System, "items_processed", itemCount);
}
```

---

*This document reflects the current state of the Gecko framework. Update as the API evolves.*
