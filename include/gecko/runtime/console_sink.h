#pragma once

#include "gecko/core/log.h"

namespace gecko::runtime {

class ConsoleSink final : public ILogSink {
public:
  virtual void Write(const LogMessage &message) noexcept override;
};

} // namespace gecko::runtime
