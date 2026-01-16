#pragma once

// System includes first (alphabetical order)
#include <cstdarg>

// Project includes second (alphabetical order)
#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

namespace gecko {

enum class LogLevel : u8
{
  Trace,  // Default: most verbose level ensures maximum information
  Debug,
  Info,
  Warn,
  Error,
  Fatal
};

GECKO_API inline const char* LevelName(LogLevel level)
{
  switch (level)
  {
  case LogLevel::Trace:
    return "TRACE";
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO ";
  case LogLevel::Warn:
    return "WARN ";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Fatal:
    return "FATAL";
  default:
    break;
  }
  return "?";
}

struct LogMessage
{
  LogLevel Level {LogLevel::Trace};
  Label MessageLabel {};
  u64 TimeNs {0};
  u32 ThreadId {0};
  const char* Text {nullptr};
};

struct ILogSink
{
  GECKO_API virtual ~ILogSink() = default;
  GECKO_API virtual void Write(const LogMessage& message) noexcept = 0;
};

struct ILogger
{
  GECKO_API virtual ~ILogger() = default;

  GECKO_API virtual void LogV(LogLevel level, Label label, const char* fmt,
                              va_list) noexcept = 0;

  GECKO_API inline void Log(LogLevel level, Label label, const char* fmt, ...)
  {
    va_list ap;
    va_start(ap, fmt);
    LogV(level, label, fmt, ap);
    va_end(ap);
  }

  GECKO_API virtual void AddSink(ILogSink* sink) noexcept = 0;
  GECKO_API virtual void SetLevel(LogLevel level) noexcept = 0;
  GECKO_API virtual LogLevel Level() const noexcept = 0;

  GECKO_API virtual void Flush() noexcept = 0;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;
};

GECKO_API ILogger* GetLogger() noexcept;

}  // namespace gecko

#ifndef GECKO_LOGGING
#define GECKO_LOGGING 1
#endif

#if GECKO_LOGGING
#define GECKO_LOG(lvl, label, ...)              \
  do                                            \
  {                                             \
    if (auto* logger = ::gecko::GetLogger())    \
      logger->Log((lvl), (label), __VA_ARGS__); \
  } while (0)

#define GECKO_TRACE(label, ...) \
  GECKO_LOG(::gecko::LogLevel::Trace, (label), __VA_ARGS__)
#define GECKO_DEBUG(label, ...) \
  GECKO_LOG(::gecko::LogLevel::Debug, (label), __VA_ARGS__)
#define GECKO_INFO(label, ...) \
  GECKO_LOG(::gecko::LogLevel::Info, (label), __VA_ARGS__)
#define GECKO_WARN(label, ...) \
  GECKO_LOG(::gecko::LogLevel::Warn, (label), __VA_ARGS__)
#define GECKO_ERROR(label, ...) \
  GECKO_LOG(::gecko::LogLevel::Error, (label), __VA_ARGS__)
#define GECKO_FATAL(label, ...) \
  GECKO_LOG(::gecko::LogLevel::Fatal, (label), __VA_ARGS__)
#else
#define GECKO_TRACE(cat, ...)
#define GECKO_DEBUG(cat, ...)
#define GECKO_INFO(cat, ...)
#define GECKO_WARN(cat, ...)
#define GECKO_ERROR(cat, ...)
#define GECKO_FATAL(cat, ...)
#endif

// NullLogger: No-op logger (discards all log messages)
// Use when logging is disabled or during early initialization
namespace gecko {
struct NullLogger final : ILogger
{
  GECKO_API virtual void LogV(LogLevel level, Label label, const char* fmt,
                              va_list) noexcept override;
  GECKO_API virtual void AddSink(ILogSink* sink) noexcept override;
  GECKO_API virtual void SetLevel(LogLevel level) noexcept override;
  GECKO_API virtual LogLevel Level() const noexcept override;

  GECKO_API virtual void Flush() noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

}  // namespace gecko
