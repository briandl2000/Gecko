#pragma once

#include "gecko/core/services/profiler.h"

#include <atomic>
#include <cstdio>

namespace gecko::runtime {

class CrashSafeTraceProfilerSink final : public IProfilerSink
{
public:
  explicit CrashSafeTraceProfilerSink(const char* path);
  ~CrashSafeTraceProfilerSink();

  bool IsOpen() const noexcept
  {
    return m_File != nullptr;
  }

  virtual void Write(const ProfEvent& event) noexcept override;
  virtual void WriteBatch(const ProfEvent* events,
                          size_t count) noexcept override;
  virtual void Flush() noexcept override;

private:
  std::FILE* m_File {nullptr};
  bool m_First {true};
  u64 m_Time0Ns {0};
  std::atomic<size_t> m_EventCount {0};
  static constexpr size_t FLUSH_INTERVAL = 100;  // Flush every N events

  void WriteEvent(const ProfEvent& event) noexcept;
  void WriteSeparator() noexcept;
  void EnsureValidJson() noexcept;  // Rewrite closing bracket for crash safety
  static void WriteJsonEvent(std::FILE* file, const ProfEvent& event,
                             u64 time0Ns) noexcept;
};

}  // namespace gecko::runtime
