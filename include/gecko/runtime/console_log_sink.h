#pragma once

#include "gecko/core/log.h"

namespace gecko::runtime {

class ConsoleLogSink final : public ILogSink {
public:
  virtual void Write(const LogMessage &message) noexcept override;
};

} // namespace gecko::runtime
