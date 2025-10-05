#pragma once

// System includes first (alphabetical order)
#include <cstdarg>

// Project includes second (alphabetical order)
#include "gecko/core/api.h"
#include "gecko/core/category.h"
#include "gecko/core/types.h"

namespace gecko {

enum class LogLevel : u8 {
  Trace, // Default: most verbose level ensures maximum information
  Debug,
  Info,
  Warn,
  Error,
  Fatal
};

GECKO_API inline const char *LevelName(LogLevel level) {
  switch (level) {
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

struct LogMessage {
  LogLevel Level{LogLevel::Trace};
  Category Cat{};
  u64 TimeNs{0};
  u32 ThreadId{0};
  const char *Text{nullptr};
};

struct ILogSink {
  GECKO_API virtual ~ILogSink() = default;
  GECKO_API virtual void Write(const LogMessage &message) noexcept = 0;
};

struct ILogger {
  GECKO_API virtual ~ILogger() = default;

  GECKO_API virtual void LogV(LogLevel level, Category category,
                              const char *fmt, va_list) noexcept = 0;

  GECKO_API inline void Log(LogLevel level, Category category, const char *fmt,
                            ...) {
    va_list ap;
    va_start(ap, fmt);
    LogV(level, category, fmt, ap);
    va_end(ap);
  }

  GECKO_API virtual void AddSink(ILogSink *sink) noexcept = 0;
  GECKO_API virtual void SetLevel(LogLevel level) noexcept = 0;
  GECKO_API virtual LogLevel Level() const noexcept = 0;

  GECKO_API virtual void Flush() noexcept = 0;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;
};

GECKO_API ILogger *GetLogger() noexcept;

} // namespace gecko

#ifndef GECKO_LOGGING
#define GECKO_LOGGING 1
#endif

#if GECKO_LOGGING
#define GECKO_LOG(lvl, cat, ...)                                               \
  do {                                                                         \
    if (auto *logger = ::gecko::GetLogger())                                   \
      logger->Log((lvl), (cat), __VA_ARGS__);                                  \
  } while (0)

#define GECKO_TRACE(cat, ...)                                                  \
  GECKO_LOG(::gecko::LogLevel::Trace, (cat), __VA_ARGS__)
#define GECKO_DEBUG(cat, ...)                                                  \
  GECKO_LOG(::gecko::LogLevel::Debug, (cat), __VA_ARGS__)
#define GECKO_INFO(cat, ...)                                                   \
  GECKO_LOG(::gecko::LogLevel::Info, (cat), __VA_ARGS__)
#define GECKO_WARN(cat, ...)                                                   \
  GECKO_LOG(::gecko::LogLevel::Warn, (cat), __VA_ARGS__)
#define GECKO_ERROR(cat, ...)                                                  \
  GECKO_LOG(::gecko::LogLevel::Error, (cat), __VA_ARGS__)
#define GECKO_FATAL(cat, ...)                                                  \
  GECKO_LOG(::gecko::LogLevel::Fatal, (cat), __VA_ARGS__)
#else
#define GECKO_TRACE(cat, ...)
#define GECKO_DEBUG(cat, ...)
#define GECKO_INFO(cat, ...)
#define GECKO_WARN(cat, ...)
#define GECKO_ERROR(cat, ...)
#define GECKO_FATAL(cat, ...)
#endif
