#pragma once

#include <cstdarg>

#include "types.h"
#include "category.h"

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

  struct LogMessage
  {
    LogLevel Level;
    Category Category;
    u64 TimeNs;
    u32 ThreadId;
    const char* Text;
  };

  struct ILogSink
  {
    virtual ~ILogSink() = default;
    virtual void Write(const LogMessage& message) noexcept = 0;
  };

  struct ILogger
  {
    virtual ~ILogger() = default;

    virtual void LogV(LogLevel level, Category category, const char* fmt, va_list) noexcept = 0;

    inline void Log(LogLevel level, Category category, const char* fmt, ...)
    {
      va_list ap;
      va_start(ap, fmt);
      LogV(level, category, fmt, ap);
      va_end(ap);
    }

    virtual void AddSink(ILogSink* sink) noexcept = 0;
    virtual void SetLevel(LogLevel level) noexcept = 0;
    virtual LogLevel Level() const noexcept = 0;

    virtual void Flush() noexcept = 0;

  };

  ILogger* GetLogger() noexcept;

}

#ifndef GECKO_LOGGING
#define GECKO_LOGGING 1
#endif

#if GECKO_LOGGING
  #define GECKO_LOG(lvl, cat, ...)                 \
    do { if (auto* logger = ::gecko::GetLogger())  \
      logger->Log((lvl), (cat), __VA_ARGS__);      \
    } while (0)

  #define GECKO_TRACE(cat, ...) GECKO_LOG(::gecko::LogLevel::Trace, (cat), __VA_ARGS__)
  #define GECKO_DEBUG(cat, ...) GECKO_LOG(::gecko::LogLevel::Debug, (cat), __VA_ARGS__)
  #define GECKO_INFO(cat,  ...) GECKO_LOG(::gecko::LogLevel::Info,  (cat), __VA_ARGS__)
  #define GECKO_WARN(cat,  ...) GECKO_LOG(::gecko::LogLevel::Warn,  (cat), __VA_ARGS__)
  #define GECKO_ERROR(cat, ...) GECKO_LOG(::gecko::LogLevel::Error, (cat), __VA_ARGS__)
  #define GECKO_FATAL(cat, ...) GECKO_LOG(::gecko::LogLevel::Fatal, (cat), __VA_ARGS__)
#elif
  #define GECKO_TRACE(cat, ...)
  #define GECKO_DEBUG(cat, ...)
  #define GECKO_INFO(cat,  ...)
  #define GECKO_WARN(cat,  ...)
  #define GECKO_ERROR(cat, ...)
  #define GECKO_FATAL(cat, ...)
#endif
