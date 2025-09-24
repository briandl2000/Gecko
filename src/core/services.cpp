#include "gecko/core/services.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "gecko/core/core.h"

namespace gecko { 

#pragma region(minimal defaults) // ------------------------------------------------
  
  
  void* SystemAllocator::Alloc(u64 size, u32 alignment, Category category) noexcept
  {
    GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
    GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");
    
    if (alignment <= alignof(std::max_align_t)) 
    {
      return std::malloc(static_cast<size_t>(size));
    }
    
#if defined(_MSC_VER)
    return _aligned_malloc(static_cast<size_t>(size), alignment);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, static_cast<size_t>(size)) != 0) return nullptr;
    return ptr;
#endif
  }
    
  void SystemAllocator::Free(void* ptr, u64 size, u32 alignment, Category category) noexcept
  {
    if (!ptr) return;
#if defined (_MSC_VER)
    if (alignment > alignof(std::max_align_t))
    {
      _aligned_free(ptr);
      return;
    }
#endif
    std::free(ptr);
  } 
  bool SystemAllocator::Init() noexcept { return true; }
  void SystemAllocator::Shutdown() noexcept { }

  void NullProfiler::Emit(const ProfEvent& event) noexcept {}
  u64 NullProfiler::NowNs() const noexcept { return 0; }
  bool NullProfiler::Init() noexcept { return true; }
  void NullProfiler::Shutdown() noexcept { }

 
  void NullLogger::LogV(LogLevel level, Category category, const char* fmt, va_list) noexcept {}
  void NullLogger::AddSink(ILogSink* sink) noexcept {}
  void NullLogger::SetLevel(LogLevel level) noexcept {} 
  LogLevel NullLogger::Level() const noexcept { return ::gecko::LogLevel::Info; } 
  void NullLogger::Flush() noexcept {}
  bool NullLogger::Init() noexcept { return true; }
  void NullLogger::Shutdown() noexcept { }

  static SystemAllocator s_SystemAllocator;
  static NullProfiler s_NullProfiler;
  static NullLogger s_NullLogger;


#pragma endregion //----------------------------------------------------------------

  static std::atomic<IAllocator*> g_Allocator { &s_SystemAllocator };
  static std::atomic<IProfiler*> g_Profiler { &s_NullProfiler };
  static std::atomic<ILogger*> g_Logger { &s_NullLogger };
  static std::atomic<bool> g_Installed { false };

  bool InstallServices(const Services& service) noexcept 
  {
    if (g_Installed.load(std::memory_order_relaxed)) 
    {
      std::fputs("[Gecko] Services are already installed!\n", stderr);
      return false; 
    }

    // Initialize services first before storing them
    if (service.Allocator)
    {
      if (!service.Allocator->Init()) 
      {
        std::fputs("[Gecko] Allocator failed to initialize!\n", stderr);
        GECKO_ASSERT(false);
        return false;
      }
      g_Allocator.store(service.Allocator, std::memory_order_release);
    }

    if (service.Profiler)
    {
      if (!service.Profiler->Init()) 
      {
        std::fputs("[Gecko] Profiler failed to initialize!\n", stderr);
        GECKO_ASSERT(false);
        return false;
      }
      g_Profiler.store(service.Profiler, std::memory_order_release);
    }

    if (service.Logger)
    {
      if (!service.Logger->Init()) 
      {
        std::fputs("[Gecko] Logger failed to initialize!\n", stderr);
        GECKO_ASSERT(false);
        return false;
      }
      g_Logger.store(service.Logger, std::memory_order_release);
    }

    g_Installed.store(true, std::memory_order_release);

    return true;
  }

  void UninstallServices() noexcept 
  { 
    g_Logger.load(std::memory_order_relaxed)->Shutdown();
    g_Profiler.load(std::memory_order_relaxed)->Shutdown();
    g_Allocator.load(std::memory_order_relaxed)->Shutdown();
    
    g_Allocator.store(&s_SystemAllocator, std::memory_order_release);
    g_Profiler.store(&s_NullProfiler, std::memory_order_release);
    g_Logger.store(&s_NullLogger, std::memory_order_release);
    g_Installed.store(false, std::memory_order_release);
  }

  IAllocator* GetAllocator() noexcept { return g_Allocator.load(std::memory_order_acquire); }
  IProfiler* GetProfiler() noexcept { return g_Profiler.load(std::memory_order_acquire); }
  ILogger* GetLogger() noexcept { return g_Logger.load(std::memory_order_acquire); }

  bool IsServicesInstalled() noexcept { return g_Installed.load(std::memory_order_acquire); }

  bool ValidateServices(bool fatalOnFail) noexcept
  {
    bool ok = true;
    if (!GetAllocator()) ok = false;
    if (!GetProfiler()) ok = false;
    if (!GetLogger()) ok = false;
    
#if GECKO_REQUIRE_INSTALL
    if (!IsServicesInstalled()) ok = false; 
#endif

    if (!ok && fatalOnFail)
    {
      std::fputs("[Gecko] Services not properly installed. Call gecko::InstallServices early in main().\n", stderr);
      GECKO_ASSERT(false);
      return false;
    }

    return ok;
  }
}
