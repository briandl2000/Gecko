#pragma once
#include "Defines.h"

#include <string>

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1

// Disable debug and trace logging for release builds.
#ifdef DEBUG
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1
#else
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif

namespace Gecko { namespace Logger
{
  enum eLogLevel
  {
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
  };

  bool Init();
  void Shutdown();
  void LogOutput(eLogLevel level, std::string message, ...);

  void ConsoleWrite(char* msg, Logger::eLogLevel level);
} }

#ifndef LOG_FATAL
#define LOG_FATAL(message, ...) Gecko::Logger::LogOutput(Gecko::Logger::eLogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__);
#endif


#ifndef LOG_ERROR
// Logs an error-level message.
#define LOG_ERROR(message, ...) Gecko::Logger::LogOutput(Gecko::Logger::eLogLevel::LOG_LEVEL_ERROR, message, ##__VA_ARGS__);
#endif

#if LOG_WARN_ENABLED == 1
// Logs a warning-level message.
#define LOG_WARN(message, ...) Gecko::Logger::LogOutput(Gecko::Logger::eLogLevel::LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_WARN_ENABLED != 1
#define LOG_WARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
// Logs a info-level message.
#define LOG_INFO(message, ...) Gecko::Logger::LogOutput(Gecko::Logger::eLogLevel::LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_INFO_ENABLED != 1
#define LOG_INFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
// Logs a debug-level message.
#define LOG_DEBUG(message, ...) Gecko::Logger::LogOutput(Gecko::Logger::eLogLevel::LOG_LEVEL_DEBUG, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_DEBUG_ENABLED != 1
#define LOG_DEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
// Logs a trace-level message.
#define LOG_TRACE(message, ...) Gecko::Logger::LogOutput(Gecko::Logger::eLogLevel::LOG_LEVEL_TRACE, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_TRACE_ENABLED != 1
#define LOG_TRACE(message, ...)
#endif
