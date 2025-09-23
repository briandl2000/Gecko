#pragma once
#include "api.h"

#include "log.h"
#include "memory.h"
#include "profiler.h"

namespace gecko {
  
  struct Services
  {
    IAllocator* Allocator = nullptr;
    IProfiler* Profiler = nullptr;
    ILogger* Logger = nullptr;
  };

  GECKO_API void InstallServices(const Services& service) noexcept;

  GECKO_API void UninstallServices() noexcept;


  GECKO_API IProfiler* GetProfiler() noexcept;
  GECKO_API IAllocator* GetAllocator() noexcept;

  bool IsServicesInstalled() noexcept;

  bool ValidateServices(bool fatalOnFail) noexcept;

  // Default service structs
  
  struct SystemAllocator final : IAllocator 
  {
    virtual void* Alloc(u64 size, u32 alignment, Category category) noexcept override; 
    
    virtual void Free(void* ptr, u64 size, u32 alignment, Category category) noexcept override;
  };

  struct NullProfiler final : IProfiler
  {
    virtual void Emit(const ProfEvent& event) noexcept override;
    virtual u64 NowNs() const noexcept override;
  };

  struct NullLogger final : ILogger
  {
    virtual void LogV(LogLevel level, Category category, const char* fmt, va_list) noexcept override;
    virtual void AddSink(ILogSink* sink) noexcept override;
    virtual void SetLevel(LogLevel level) noexcept override; 
    virtual LogLevel Level() const noexcept override; 

    virtual void Flush() noexcept override; 
  };

}
#ifndef GECKO_REQUIRE_INSTALL
  #define GECKO_REQUIRE_INSTALL 1
#endif
