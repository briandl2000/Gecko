#pragma once

#include <vector>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <functional>

#include "gecko/core/log.h"
#include "gecko/core/types.h"

namespace gecko::runtime {

  class ImmediateLogger final : public ILogger
  {
  public:
    ImmediateLogger() = default;
    virtual ~ImmediateLogger() = default;

    virtual void LogV(LogLevel level, Category category, const char* fmt, va_list ap) noexcept override;

    virtual void AddSink(ILogSink* sink) noexcept override;
    virtual void SetLevel(LogLevel level) noexcept override { m_Level = level; }
    virtual LogLevel Level() const noexcept override { return m_Level; }

    virtual void Flush() noexcept override;

  private:
    std::vector<ILogSink*> m_Sinks;
    std::mutex m_SinkMutex;
    LogLevel m_Level = LogLevel::Info;

    static u64 NowNs() noexcept;
    static u32 ThreadId() noexcept;
  };

}
