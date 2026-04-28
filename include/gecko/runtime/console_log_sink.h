#pragma once

/// @file
/// `ConsoleLogSink` -- `ILogSink` that writes formatted messages to the
/// terminal via `gecko::platform::Print`.

#include "gecko/core/services/log.h"

namespace gecko::runtime {

/// Writes log messages to stdout / stderr with terminal colours
/// matching the message's `LogLevel`.
class ConsoleLogSink final : public ILogSink
{
public:
  virtual void Write(const LogMessage& message) noexcept override;
};

}  // namespace gecko::runtime
