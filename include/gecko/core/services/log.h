#pragma once

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/sink_registration.h"
#include "gecko/core/types.h"

#include <cstdarg>

namespace gecko {

enum class LogLevel : u8
{
  Trace,
  Debug,
  Info,
  Warn,
  Error,
  Fatal
};

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

// Aligned to speed up LogMessages
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)  // structure was padded due to alignment
#endif
struct alignas(64) LogMessage
{
  u64 TimeNs {0};
  const char* Text {nullptr};
  Label MessageLabel {};

  u32 ThreadId {0};

  LogLevel Level {LogLevel::Trace};
};

static_assert(sizeof(LogMessage) == 64,
              "LogMessage must be 64 bytes (cache line size)");
static_assert(alignof(LogMessage) == 64,
              "LogMessage must be cache-line aligned");
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Forward declare for RegisteredSink
struct ILogger;

struct ILogSink : public RegisteredSink<ILogSink, ILogger>
{
  GECKO_API virtual ~ILogSink() = default;
  GECKO_API virtual void Write(const LogMessage& message) noexcept = 0;
};

struct ILogger
{
  GECKO_API virtual ~ILogger() = default;

  GECKO_API virtual void LogV(LogLevel level, Label label, const char* fmt,
                              va_list) noexcept = 0;

  inline void Log(LogLevel level, Label label, const char* fmt, ...)
  {
    va_list ap;
    va_start(ap, fmt);
    LogV(level, label, fmt, ap);
    va_end(ap);
  }

  // Internal: called by RegisteredSink
  GECKO_API virtual void AddSink(ILogSink* sink) noexcept = 0;
  GECKO_API virtual void RemoveSink(ILogSink* sink) noexcept = 0;

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
  GECKO_API virtual void LogV(LogLevel level, Label label, const char* fmt,
                              va_list) noexcept override;
  GECKO_API virtual void AddSink(ILogSink* sink) noexcept override;
  GECKO_API virtual void RemoveSink(ILogSink* sink) noexcept override;
  GECKO_API virtual void SetLevel(LogLevel level) noexcept override;
  GECKO_API virtual LogLevel Level() const noexcept override;

  GECKO_API virtual void Flush() noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

}  // namespace gecko
