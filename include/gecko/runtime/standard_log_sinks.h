#pragma once

/// @file
/// `StandardLogSinks` -- RAII helper that attaches a `ConsoleLogSink`
/// and a `FileLogSink` to the active logger and detaches them on
/// destruction.

#include "gecko/core/services/log.h"
#include "gecko/runtime/console_log_sink.h"
#include "gecko/runtime/file_log_sink.h"

namespace gecko::runtime {

/// Attaches a console sink and a file sink to the active logger.
///
/// Construct after the engine has booted (so `GetLogger()` returns the
/// real logger, not the null fallback). Both sinks are unregistered in
/// the destructor.
///
/// Typical use:
///
/// ```cpp
/// auto engine = Engine::Create({...});
/// runtime::StandardLogSinks sinks;       // log.txt, LogLevel::Info
/// // ... run ...
/// // sinks goes out of scope before engine.reset().
/// ```
///
/// If the active logger when the constructor runs is the process-wide
/// null logger (engine not booted), `Ok()` returns `false` and the
/// destructor is a no-op.
class StandardLogSinks
{
public:
  /// @param logFilePath Path to the log file, opened for append.
  /// @param level Logger level set after attaching the sinks.
  explicit StandardLogSinks(const char* logFilePath = "log.txt",
                            LogLevel level = LogLevel::Info) noexcept;
  ~StandardLogSinks() noexcept;

  StandardLogSinks(const StandardLogSinks&) = delete;
  StandardLogSinks& operator=(const StandardLogSinks&) = delete;
  StandardLogSinks(StandardLogSinks&&) = delete;
  StandardLogSinks& operator=(StandardLogSinks&&) = delete;

  /// @return `true` if both sinks were registered on a real logger.
  [[nodiscard]] bool Ok() const noexcept
  {
    return m_Attached;
  }
  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_Attached;
  }

  /// @return The owned console sink (still attached while `Ok()`).
  [[nodiscard]] ConsoleLogSink& Console() noexcept
  {
    return m_Console;
  }
  /// @return The owned file sink (still attached while `Ok()`).
  [[nodiscard]] FileLogSink& File() noexcept
  {
    return m_File;
  }

private:
  ConsoleLogSink m_Console;
  FileLogSink m_File;
  bool m_Attached = false;
};

}  // namespace gecko::runtime
