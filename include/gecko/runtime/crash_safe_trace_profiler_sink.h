#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/platform_io.h"

#include <atomic>

namespace gecko::runtime {

class CrashSafeTraceProfilerSink final : public IProfilerSink
{
public:
  explicit CrashSafeTraceProfilerSink(const char* path);
  ~CrashSafeTraceProfilerSink();

  bool IsOpen() const noexcept
  {
    return m_Writer != nullptr;
  }

  virtual void Write(const ProfEvent& event) noexcept override;
  virtual void WriteBatch(
      ::std::span<const ProfEvent> events) noexcept override;
  virtual void Flush() noexcept override;

private:
  ::gecko::Unique<::gecko::platform::FileWriter> m_Writer {};
  bool m_First {true};
  u64 m_Time0Ns {0};
  std::atomic<size_t> m_EventCount {0};
  static constexpr size_t FLUSH_INTERVAL = 100;

  void WriteEvent(const ProfEvent& event) noexcept;
  void WriteSeparator() noexcept;
  void EnsureValidJson() noexcept;
};

}  // namespace gecko::runtime
