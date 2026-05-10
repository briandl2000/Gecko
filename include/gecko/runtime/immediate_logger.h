#pragma once

/// @file
/// `ImmediateLogger` -- synchronous `ILogger` implementation that
/// forwards each call directly to its sinks.

#include "gecko/core/services/log.h"

#include <mutex>
#include <vector>

namespace gecko::runtime {

/// Synchronous logger. Each `LogV` call dispatches to every attached
/// sink on the calling thread before returning. Suitable for tools and
/// tests; for production builds prefer `RingLogger`.
class ImmediateLogger final : public ILogger
{
public:
  ImmediateLogger() = default;
  virtual ~ImmediateLogger() = default;

  virtual void LogV(LogLevel level, Label label, const char* fmt, va_list ap) noexcept override;
  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  virtual void AddSink(ILogSink* sink) noexcept override;
  virtual void RemoveSink(ILogSink* sink) noexcept override;
  virtual void SetLevel(LogLevel level) noexcept override
  {
    m_Level = level;
  }
  virtual LogLevel Level() const noexcept override
  {
    return m_Level;
  }

  virtual void Flush() noexcept override;

  /// Toggle a mutex around `LogV` and `Flush` so multiple threads can
  /// log safely. Off by default for single-threaded use.
  void SetThreadSafe(bool on) noexcept
  {
    m_ThreadSafe = on;
  }

private:
  std::vector<ILogSink*> m_Sinks;
  std::mutex m_Mutex;
  LogLevel m_Level {LogLevel::Info};
  bool m_ThreadSafe {false};
};

}  // namespace gecko::runtime
