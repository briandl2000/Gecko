#pragma once

/// @file
/// `ImmediateLogger` -- synchronous `ILogger` implementation that
/// forwards each call directly to its sinks.

#include "gecko/core/ptr.h"
#include "gecko/core/services/log.h"

namespace gecko::runtime {

/// Synchronous logger. Each `LogV` call dispatches to every attached
/// sink on the calling thread before returning. Suitable for tools and
/// tests; for production builds prefer `RingLogger`.
class ImmediateLogger final : public ILogger
{
public:
  ImmediateLogger() noexcept;
  virtual ~ImmediateLogger() noexcept;

  virtual void LogV(LogLevel level, Label label, const char* fmt, va_list ap) noexcept override;
  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  virtual void AddSink(ILogSink* sink) noexcept override;
  virtual void RemoveSink(ILogSink* sink) noexcept override;
  virtual void SetLevel(LogLevel level) noexcept override;
  virtual LogLevel Level() const noexcept override;

  virtual void Flush() noexcept override;

  /// Toggle a mutex around `LogV` and `Flush` so multiple threads can
  /// log safely. Off by default for single-threaded use.
  void SetThreadSafe(bool on) noexcept;

private:
  struct Impl;
  ::gecko::Unique<Impl> m_Impl;
};

}  // namespace gecko::runtime
