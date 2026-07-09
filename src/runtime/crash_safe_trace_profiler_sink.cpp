#include "gecko/runtime/crash_safe_trace_profiler_sink.h"

#include "gecko/core/assert.h"
#include "gecko/platform/platform_io.h"
#include "private/chrome_trace_format.h"
#include "private/file_writer_format.h"

#include <atomic>
#include <new>

namespace gecko::runtime {

namespace {

using ::gecko::runtime::detail::WriteChromeTraceEvent;
using ::gecko::runtime::detail::WriteFmt;

}  // namespace

struct CrashSafeTraceProfilerSink::Impl
{
  ::gecko::Unique<::gecko::platform::FileWriter> Writer {};
  bool First {true};
  u64 Time0Ns {0};
  std::atomic<size_t> EventCount {0};
  static constexpr size_t FlushInterval = 100;
};

CrashSafeTraceProfilerSink::CrashSafeTraceProfilerSink(const char* path)
{
  GECKO_ASSERT(path && "Trace file path cannot be null");

  m_Impl.reset(new (::std::nothrow) Impl());
  if (!m_Impl)
    return;

  m_Impl->Writer = ::gecko::platform::OpenWrite(path, ::gecko::platform::WriteMode::Truncate);

  if (m_Impl->Writer)
  {
    m_Impl->Writer->WriteString("{\"traceEvents\":[]}");
    m_Impl->Writer->Flush();
    m_Impl->First = true;
    m_Impl->Time0Ns = 0;
    m_Impl->EventCount.store(0, std::memory_order_relaxed);
  }
}

CrashSafeTraceProfilerSink::~CrashSafeTraceProfilerSink()
{
  Unregister();
  if (m_Impl && m_Impl->Writer)
  {
    EnsureValidJson();
    m_Impl->Writer.reset();
  }
}

bool CrashSafeTraceProfilerSink::IsOpen() const noexcept
{
  return m_Impl && m_Impl->Writer;
}

void CrashSafeTraceProfilerSink::Write(const ProfEvent& event) noexcept
{
  if (!m_Impl || !m_Impl->Writer)
    return;

  WriteEvent(event);

  size_t count = m_Impl->EventCount.fetch_add(1, std::memory_order_relaxed) + 1;
  if (count % Impl::FlushInterval == 0)
    EnsureValidJson();
}

void CrashSafeTraceProfilerSink::WriteBatch(::gecko::Span<const ProfEvent> events) noexcept
{
  if (!m_Impl || !m_Impl->Writer || events.empty())
    return;

  for (const ProfEvent& e : events)
    WriteEvent(e);

  m_Impl->EventCount.fetch_add(events.size(), std::memory_order_relaxed);
  EnsureValidJson();
}

void CrashSafeTraceProfilerSink::WriteEvent(const ProfEvent& event) noexcept
{
  if (m_Impl->Time0Ns == 0)
    m_Impl->Time0Ns = event.TimestampNs;

  // Seek back to overwrite the closing ]} and insert new event.
  m_Impl->Writer->Seek(-2, /*fromEnd=*/true);

  WriteSeparator();
  WriteChromeTraceEvent(m_Impl->Writer.get(), event, m_Impl->Time0Ns);
  m_Impl->Writer->WriteString("]}");
}

void CrashSafeTraceProfilerSink::WriteSeparator() noexcept
{
  if (!m_Impl->First)
    m_Impl->Writer->WriteString(",");
  m_Impl->First = false;
}

void CrashSafeTraceProfilerSink::EnsureValidJson() noexcept
{
  if (m_Impl && m_Impl->Writer)
    m_Impl->Writer->Flush();
}

void CrashSafeTraceProfilerSink::Flush() noexcept
{
  EnsureValidJson();
}

}  // namespace gecko::runtime
