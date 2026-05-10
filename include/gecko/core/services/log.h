#pragma once

/// @file
/// Engine-wide logging service interface.
///
/// Use the `GECKO_TRACE` / `GECKO_DEBUG` / `GECKO_INFO` / `GECKO_WARN` /
/// `GECKO_ERROR` / `GECKO_FATAL` macros for application logging -- they
/// route through `GetLogger()` and compile out when `GECKO_LOGGING` is
/// defined to `0`. Concrete loggers (e.g. `ImmediateLogger`,
/// `RingLogger`) live in the runtime module.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/sink_registration.h"
#include "gecko/core/types.h"

#include <cstdarg>

namespace gecko {

/// Severity of a log message, ordered from least to most severe.
enum class LogLevel : u8
{
  Trace,  ///< Verbose tracing for tight loops.
  Debug,  ///< Development diagnostics.
  Info,   ///< Normal operational messages.
  Warn,   ///< Recoverable anomaly.
  Error,  ///< Failed operation; execution continues.
  Fatal   ///< Unrecoverable; usually followed by abort.
};

/// Map a `LogLevel` to its uppercase short name.
/// @param level Severity to translate.
/// @return Stable string such as `"INFO"` (never null).
inline const char* LevelName(LogLevel level)
{
  switch (level)
  {
  case LogLevel::Trace:
    return "TRACE";
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Fatal:
    return "FATAL";
  default:
    break;
  }
  return "?";
}

// Aligned for cache-line performance
/// One log record passed to sinks.
///
/// Layout is hand-tuned to fit exactly one 64-byte cache line so that
/// `RingLogger` can move records between threads without straddling
/// lines. Member declaration order is significant -- do not reorder.
struct alignas(64) LogMessage
{
  u64 TimeNs {0};              ///< Timestamp, nanoseconds since process start.
  const char* Text {nullptr};  ///< Pre-formatted message text (sink-owned lifetime varies).
  Label MessageLabel {};       ///< Origin label (e.g. `app.module.subsystem`).

  u32 ThreadId {0};  ///< Logical thread id of the emitter.

  LogLevel Level {LogLevel::Trace};  ///< Severity bucket.
};

static_assert(sizeof(LogMessage) == 64, "LogMessage must be 64 bytes (cache line size)");
static_assert(alignof(LogMessage) == 64, "LogMessage must be cache-line aligned");

// Forward declare for RegisteredSink
struct ILogger;

/// Sink that receives formatted log messages from an `ILogger`.
///
/// Inherits the `RegisteredSink` CRTP helper, which provides
/// `RegisterWith(logger)` / `Unregister()` plumbing.
struct ILogSink : public RegisteredSink<ILogSink, ILogger>
{
  GECKO_API virtual ~ILogSink() = default;
  /// Receive a formatted log record.
  /// Called from arbitrary threads depending on the logger
  /// implementation; sinks must be thread-safe.
  /// @param message The record to write.
  GECKO_API virtual void Write(const LogMessage& message) noexcept = 0;
};

/// Logger service interface. The active logger is reachable via
/// `GetLogger()` and is normally driven through the `GECKO_*` macros.
struct ILogger
{
  GECKO_API virtual ~ILogger() = default;

  /// Format and emit a log message using `vprintf`-style arguments.
  /// @param level Severity bucket.
  /// @param label Origin label (e.g. `app.module.subsystem`).
  /// @param fmt `printf`-style format string.
  GECKO_API virtual void LogV(LogLevel level, Label label, const char* fmt, va_list) noexcept = 0;

  /// Variadic convenience wrapper around `LogV`.
  inline void Log(LogLevel level, Label label, const char* fmt, ...)
  {
    va_list ap;
    va_start(ap, fmt);
    LogV(level, label, fmt, ap);
    va_end(ap);
  }

  /// Attach a sink. Prefer `ILogSink::RegisterWith(logger)`.
  /// @param sink Sink to attach (must outlive the logger or be
  ///        unregistered first).
  GECKO_API virtual void AddSink(ILogSink* sink) noexcept = 0;
  /// Detach a sink. Prefer `ILogSink::Unregister()`.
  /// @param sink Previously registered sink.
  GECKO_API virtual void RemoveSink(ILogSink* sink) noexcept = 0;

  /// Drop messages strictly below `level` before reaching sinks.
  /// @param level Minimum level to forward.
  GECKO_API virtual void SetLevel(LogLevel level) noexcept = 0;
  /// @return Current minimum level.
  GECKO_API virtual LogLevel Level() const noexcept = 0;

  /// Block until any buffered messages have been delivered to sinks.
  /// No-op for synchronous loggers.
  GECKO_API virtual void Flush() noexcept = 0;

  /// One-time setup. Called by the engine when the logger is
  /// installed.
  GECKO_API virtual bool Init() noexcept = 0;
  /// Counterpart to `Init`. Sinks should be detached before this
  /// runs.
  GECKO_API virtual void Shutdown() noexcept = 0;
};

/// @return The currently installed logger, or a `NullLogger` instance
///         if none has been installed. Never null.
GECKO_API ILogger* GetLogger() noexcept;

}  // namespace gecko

#ifndef GECKO_LOGGING
#define GECKO_LOGGING 1
#endif

#if GECKO_LOGGING
#define GECKO_LOG(lvl, label, ...)                  \
  do                                                \
  {                                                 \
    if (auto* gk_logger_ = ::gecko::GetLogger())    \
      gk_logger_->Log((lvl), (label), __VA_ARGS__); \
  } while (0)

#define GECKO_TRACE(label, ...) GECKO_LOG(::gecko::LogLevel::Trace, (label), __VA_ARGS__)
#define GECKO_DEBUG(label, ...) GECKO_LOG(::gecko::LogLevel::Debug, (label), __VA_ARGS__)
#define GECKO_INFO(label, ...) GECKO_LOG(::gecko::LogLevel::Info, (label), __VA_ARGS__)
#define GECKO_WARN(label, ...) GECKO_LOG(::gecko::LogLevel::Warn, (label), __VA_ARGS__)
#define GECKO_ERROR(label, ...) GECKO_LOG(::gecko::LogLevel::Error, (label), __VA_ARGS__)
#define GECKO_FATAL(label, ...) GECKO_LOG(::gecko::LogLevel::Fatal, (label), __VA_ARGS__)
#else
#define GECKO_TRACE(label, ...)
#define GECKO_DEBUG(label, ...)
#define GECKO_INFO(label, ...)
#define GECKO_WARN(label, ...)
#define GECKO_ERROR(label, ...)
#define GECKO_FATAL(label, ...)
#endif

namespace gecko {
struct NullLogger final : ILogger
{
  GECKO_API virtual void LogV(LogLevel level, Label label, const char* fmt, va_list) noexcept override;
  GECKO_API virtual void AddSink(ILogSink* sink) noexcept override;
  GECKO_API virtual void RemoveSink(ILogSink* sink) noexcept override;
  GECKO_API virtual void SetLevel(LogLevel level) noexcept override;
  GECKO_API virtual LogLevel Level() const noexcept override;

  GECKO_API virtual void Flush() noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

}  // namespace gecko
