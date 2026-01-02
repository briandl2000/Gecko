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
- **Public Member Variables**: PascalCase (`Level`, `Category`, `TimeNs`)
- **Private Member Variables**: m_ prefix (`m_File`, `m_ThreadSafe`, `m_Level`)
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

#### Initialization Patterns
Use brace initialization `{}` for struct/class initialization:

```cpp
// Struct member initialization with explicit logical defaults
struct LogMessage
{
  LogLevel Level { LogLevel::Trace };  // Explicit enum value
  Category Category { };               // Default-constructed (struct/class)
  u64 TimeNs { 0 };                   // Explicit zero
  u32 ThreadId { 0 };                 // Explicit zero
  const char* Text { nullptr };       // Explicit nullptr
};

// Variable initialization within scopes (use = assignment)
void Function()
{
  LogMessage message = { };          // Struct initialization with =
  bool initialized = false;          // Explicit false with =
  int count = 0;                    // Explicit zero with =
  float ratio = 0.0f;               // Explicit zero with =
  const char* name = nullptr;       // Explicit nullptr with =
}

// Member initialization in constructors (use { } in member initializer list)
class MyClass
{
public:
  MyClass() : m_Value(0), m_Initialized(false) { }
  
private:
  int m_Value { 0 };           // Member initialization with { }
  bool m_Initialized { false }; // Member initialization with { }
};
```

**Important**: Use different initialization syntax based on context:

**Member Variables (in class/struct declarations)**:
- Syntax: `varname { value };` with space before opening brace
- Examples: `int m_Count { 0 };`, `bool m_Active { false };`

**Local Variables (within function scopes)**:
- Syntax: `varname = value;` using assignment operator
- Examples: `int count = 0;`, `bool active = false;`, `MyStruct data = { };`

**Constructor Member Initializer Lists**:
- Syntax: `varname { value }` in initializer list
- Example: `MyClass() : m_Value { 0 }, m_Name { nullptr } { }`

**Default Values by Type**:
- **Numeric types**: `0` - Explicit zero
- **Pointers**: `nullptr` - Explicit nullptr  
- **Booleans**: `false` - Explicit false (or `true` if appropriate)
- **Enums**: `EnumType::DefaultValue` - Explicit enum value
- **Structs/Classes**: `{ }` - Default/aggregate initialization

#### Enum Design Guidelines

```cpp
// Regular enums: logical safe default first
enum class LogLevel : u8
{
  Trace,    // Safest default - maximum information
  Debug,
  Info,
  Warn,
  Error,
  Fatal
};

// Bit flag enums: use power-of-2 values
enum class FileAccess : u32
{
  None  = 0,
  Read  = gecko::Bit(0),  // 1
  Write = gecko::Bit(1),  // 2
  Exec  = gecko::Bit(2),  // 4
};

// Usage with gecko::bit.h utilities
FileAccess combined = FileAccess::Read | FileAccess::Write;
bool canRead = gecko::Any(combined & FileAccess::Read);
u32 underlying = gecko::ToUnderlying(combined);
```

**Guidelines:**
- **Regular enums**: Use logical safe default first (e.g., `Trace` for logging, `Headless` for renderers)
- **Bit flag enums**: Use `gecko::Bit(n)` for values, enable operators via `gecko::bit.h`
- **Naming**: Use context-appropriate names (`Headless`, `Disconnected`, `Pending`) instead of generic `None`

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

The Gecko framework uses a centralized service system for core functionality with a strict dependency hierarchy to prevent circular dependencies:

#### Service Dependency Hierarchy
```
Level 0: Allocator    - No dependencies
Level 1: JobSystem    - May use allocator for job storage and worker threads
Level 2: Profiler     - May use allocator and job system for background processing
Level 3: Logger       - May use allocator, job system, and profiler for performance tracking
```

**Important:** Services are initialized in dependency order (0→3) and shut down in reverse order (3→0). Services at each level can only depend on services from lower levels during initialization. This order ensures that JobSystem is available early for background processing by Profiler and Logger components.

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

### Bit Utilities

Gecko provides utilities for bit manipulation and enum flag operations:

```cpp
#include "gecko/core/bit.h"

// Bit flag enum definition
enum class RenderFlags : u32
{
  None     = 0,
  Shadows  = gecko::Bit(0),  // 1
  Lighting = gecko::Bit(1),  // 2
  PostFX   = gecko::Bit(2),  // 4
  Debug    = gecko::Bit(3),  // 8
};

// Usage examples
void SetupRenderer()
{
  // Combine flags
  RenderFlags flags = RenderFlags::Shadows | RenderFlags::Lighting;
  
  // Check if any flags are set
  if (gecko::Any(flags))
  {
    // Enable rendering
  }
  
  // Check specific flag
  if (gecko::Any(flags & RenderFlags::Shadows))
  {
    // Setup shadow mapping
  }
  
  // Add flag
  flags |= RenderFlags::PostFX;
  
  // Get underlying value
  u32 value = gecko::ToUnderlying(flags);  // For serialization, etc.
}

// Modern enum iteration (replaces Count pattern)
template<typename E>
constexpr size_t EnumCount(E maxValue)
{
  return static_cast<size_t>(gecko::ToUnderlying(maxValue)) + 1;
}

// Example usage
enum class GameState : u8 { Menu, Playing, Paused, GameOver };
constexpr size_t stateCount = EnumCount(GameState::GameOver);  // 4

// For flag enums, count set bits
template<typename E>
constexpr size_t CountSetFlags(E flags)
{
  return std::popcount(gecko::ToUnderlying(flags));
}
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

### Threading Utilities

Gecko provides cross-platform threading utilities:

```cpp
#include "gecko/core/thread.h"

// Thread identification
u32 threadId = gecko::ThisThreadId();    // Also available from profiler.h
u32 hashedId = gecko::HashThreadId();    // Consistent thread ID hash
u32 coreCount = gecko::HardwareThreadCount();

// Sleep functions
gecko::SleepMs(100);    // Sleep for 100 milliseconds
gecko::SleepUs(500);    // Sleep for 500 microseconds  
gecko::SleepNs(1000);   // Sleep for 1000 nanoseconds

// Thread cooperation
gecko::YieldThread();   // Yield current thread's time slice

// High-precision timing
gecko::SpinWaitNs(1000);        // Busy-wait for precise timing
gecko::PreciseSleepNs(50000);   // High-precision sleep using sleep + spin

// Using macros
GECKO_SLEEP_MS(10);
GECKO_YIELD();
GECKO_PRECISE_SLEEP_NS(1000000);
u32 tid = GECKO_THREAD_HASH();  // Thread ID hash
```

### Time Utilities

Gecko provides centralized time functions to ensure consistency across all services:

```cpp
#include "gecko/core/time.h"

// Get current time in nanoseconds (different clocks for different purposes)
u64 monotonic = gecko::MonotonicTimeNs();    // For profiling and time measurements
u64 highRes = gecko::HighResTimeNs();        // For high precision timing
u64 system = gecko::SystemTimeNs();          // For timestamps that correlate with system time

// Time conversion utilities
using namespace gecko::time;
u64 nsFromMs = MillisecondsToNs(100);        // Convert 100ms to nanoseconds
u64 msFromNs = NsToMilliseconds(1000000);    // Convert nanoseconds to milliseconds
double secondsF = NsToSecondsF(nanoTime);    // Convert to floating point seconds

// Using macros for common operations
u64 now = GECKO_TIME_NOW_NS();               // Monotonic time
u64 hiRes = GECKO_HIRES_TIME_NOW_NS();       // High resolution time
```

### Random Number Generation

Gecko provides thread-safe random number generation with consistent API:

```cpp
#include "gecko/core/random.h"

// Generate random integers
u32 randomU32 = gecko::RandomU32(1, 100);       // Random u32 in range [1, 100]
i32 randomI32 = gecko::RandomI32(-50, 50);      // Random i32 in range [-50, 50]
u64 randomU64 = gecko::RandomU64();             // Random u64 (full range)

// Generate random floating point numbers
float randomFloat = gecko::RandomFloat(0.0f, 1.0f);  // Random float in [0, 1)
double randomDouble = gecko::RandomDouble(-1.0, 1.0); // Random double in [-1, 1)

// Generate random bytes
u8 buffer[256];
gecko::RandomBytes(buffer, sizeof(buffer));

// Utility functions for common patterns
size_t randomIndex = gecko::random::Index(arraySize);  // Random index for array
float normalized = gecko::random::Normalized();        // Random float in [0, 1)
float signed = gecko::random::Signed();                // Random float in [-1, 1)
bool choice = gecko::RandomBool();                      // 50/50 chance
auto& chosen = gecko::random::Choose(option1, option2); // Randomly pick one

// Seed the random generator (optional - auto-seeded by default)
gecko::SeedRandom(12345);  // Use specific seed for reproducible results

// Using macros
u32 rand = GECKO_RANDOM_U32(1, 10);
float norm = GECKO_RANDOM_NORMALIZED();
bool flip = GECKO_RANDOM_BOOL();
```

### Job System

Gecko provides a flexible job system for multithreading:

```cpp
#include "gecko/core/jobs.h"
#include "gecko/runtime/thread_pool_job_system.h"

// Setup (typically in main)
gecko::runtime::ThreadPoolJobSystem jobSystem;
jobSystem.SetWorkerThreadCount(4);  // 0 = auto-detect

gecko::Services services{
  .JobSystem = &jobSystem
  // ... other services
};
GECKO_BOOT(services);

// Basic job submission
auto category = gecko::MakeCategory("compute");
auto job = []() {
  // Do work here
  GECKO_INFO(category, "Job executed");
};

auto handle = gecko::SubmitJob(job, gecko::JobPriority::Normal, category);
gecko::WaitForJob(handle);

// Job dependencies (pipeline pattern)
auto job1 = gecko::SubmitJob([]() { /* Stage 1 */ }, gecko::JobPriority::High, category);
gecko::JobHandle deps[] = { job1 };
auto job2 = gecko::SubmitJob([]() { /* Stage 2 */ }, deps, 1, gecko::JobPriority::Normal, category);
gecko::WaitForJob(job2);

// Batch operations
std::vector<gecko::JobHandle> handles;
for (int i = 0; i < 10; ++i) {
  handles.push_back(gecko::SubmitJob([i]() { /* Work */ }, gecko::JobPriority::Normal, category));
}
gecko::WaitForJobs(handles.data(), static_cast<u32>(handles.size()));

// Main thread job processing
gecko::GetJobSystem()->ProcessJobs(5);  // Process up to 5 jobs on current thread

// Using function calls directly
auto handle = gecko::SubmitJob(job, gecko::JobPriority::Normal, category);
gecko::WaitForJob(handle);
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
