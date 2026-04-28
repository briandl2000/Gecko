#pragma once

/// @file
/// `FileLogSink` -- simple `ILogSink` that appends formatted log
/// messages to a file via `gecko::platform::FileWriter`.

#include "gecko/core/ptr.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/platform_io.h"

namespace gecko::runtime {

/// Writes log messages to a file, one line per message.
class FileLogSink final : public ILogSink
{
public:
  /// Open `path` for appending. The file is created if missing.
  explicit FileLogSink(const char* path);
  ~FileLogSink();
  virtual void Write(const LogMessage& message) noexcept override;

private:
  ::gecko::Unique<::gecko::platform::FileWriter> m_Writer {};
};

}  // namespace gecko::runtime
