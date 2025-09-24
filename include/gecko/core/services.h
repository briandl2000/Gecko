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

  GECKO_API bool InstallServices(const Services& service) noexcept;

  GECKO_API void UninstallServices() noexcept;

  GECKO_API IProfiler* GetProfiler() noexcept;
  GECKO_API IAllocator* GetAllocator() noexcept;

  GECKO_API bool IsServicesInstalled() noexcept;

  GECKO_API bool ValidateServices(bool fatalOnFail) noexcept;

  // Default service structs
  
  struct SystemAllocator final : IAllocator 
  {
    GECKO_API virtual void* Alloc(u64 size, u32 alignment, Category category) noexcept override; 
    
    GECKO_API virtual void Free(void* ptr, u64 size, u32 alignment, Category category) noexcept override;

    GECKO_API virtual bool Init() noexcept override;
    GECKO_API virtual void Shutdown() noexcept override;
  };

  struct NullProfiler final : IProfiler
  {
    GECKO_API virtual void Emit(const ProfEvent& event) noexcept override;
    GECKO_API virtual u64 NowNs() const noexcept override;
 
    GECKO_API virtual bool Init() noexcept override;
    GECKO_API virtual void Shutdown() noexcept override;
  };

  struct NullLogger final : ILogger
  {
    GECKO_API virtual void LogV(LogLevel level, Category category, const char* fmt, va_list) noexcept override;
    GECKO_API virtual void AddSink(ILogSink* sink) noexcept override;
    GECKO_API virtual void SetLevel(LogLevel level) noexcept override; 
    GECKO_API virtual LogLevel Level() const noexcept override; 

    GECKO_API virtual void Flush() noexcept override; 

    GECKO_API virtual bool Init() noexcept override;
    GECKO_API virtual void Shutdown() noexcept override;
  };

}
#ifndef GECKO_REQUIRE_INSTALL
  #define GECKO_REQUIRE_INSTALL 1
#endif
