#include "gecko/runtime/async_trace_profiler_sink.h"

#include "gecko/core/assert.h"
#include "gecko/platform/platform_io.h"
#include "private/chrome_trace_format.h"
#include "private/file_writer_format.h"

#include <algorithm>
#include <chrono>

namespace gecko::runtime {

namespace {

using ::gecko::runtime::detail::WriteChromeTraceEvent;
using ::gecko::runtime::detail::WriteChromeTraceThreadName;

constexpr ::std::chrono::milliseconds c_FsyncInterval {100};

}  // namespace

AsyncTraceProfilerSink::AsyncTraceProfilerSink(const char* path)
{
  GECKO_ASSERT(path && "Trace file path cannot be null");

  m_Writer = ::gecko::platform::OpenWrite(
      path, ::gecko::platform::WriteMode::Truncate);

  if (!m_Writer)
    return;

  m_Writer->WriteString("{\"traceEvents\":[");
  m_Writer->Flush();

  m_Worker = ::std::thread([this]() { WorkerLoop(); });
}

AsyncTraceProfilerSink::~AsyncTraceProfilerSink()
{
  m_Run.store(false, ::std::memory_order_release);
  m_Cv.notify_all();
  if (m_Worker.joinable())
    m_Worker.join();

  if (m_Writer)
  {
    m_Writer->WriteString("]}");
    m_Writer->Flush();
    m_Writer.reset();
  }
}

void AsyncTraceProfilerSink::Write(const ProfEvent& event) noexcept
{
  if (!m_Writer)
    return;
  {
    ::std::lock_guard<::std::mutex> lk(m_Mu);
    m_Pending.push_back(event);
  }
  m_Cv.notify_one();
}

void AsyncTraceProfilerSink::WriteBatch(const ProfEvent* events,
                                        ::std::size_t count) noexcept
{
  if (!m_Writer || !events || count == 0)
    return;
  {
    ::std::lock_guard<::std::mutex> lk(m_Mu);
    m_Pending.insert(m_Pending.end(), events, events + count);
  }
  m_Cv.notify_one();
}

void AsyncTraceProfilerSink::Flush() noexcept
{
  if (!m_Writer)
    return;

  // Block until the worker has drained whatever was pending at call time.
  // We do this by snapshotting the current pending size and waiting for
  // the worker to consume past it. Simpler: take the lock, swap into a
  // local batch, write it ourselves (Flush is rare and synchronous by
  // contract).
  ::std::vector<ProfEvent> batch;
  {
    ::std::lock_guard<::std::mutex> lk(m_Mu);
    batch.swap(m_Pending);
  }
  DrainAndWrite(batch);
  if (m_Writer)
    m_Writer->Flush();
}

void AsyncTraceProfilerSink::WorkerLoop() noexcept
{
  ::std::vector<ProfEvent> batch;
  auto lastFsync = ::std::chrono::steady_clock::now();

  while (true)
  {
    {
      ::std::unique_lock<::std::mutex> lk(m_Mu);
      m_Cv.wait_for(lk, c_FsyncInterval, [this]() {
        return !m_Run.load(::std::memory_order_acquire) || !m_Pending.empty();
      });
      batch.swap(m_Pending);
    }

    if (!batch.empty())
      DrainAndWrite(batch);

    auto now = ::std::chrono::steady_clock::now();
    if (now - lastFsync >= c_FsyncInterval)
    {
      if (m_Writer)
        m_Writer->Flush();
      lastFsync = now;
    }

    if (!m_Run.load(::std::memory_order_acquire))
    {
      // Final drain for events queued between the wait and the flag.
      {
        ::std::lock_guard<::std::mutex> lk(m_Mu);
        batch.swap(m_Pending);
      }
      if (!batch.empty())
        DrainAndWrite(batch);
      if (m_Writer)
        m_Writer->Flush();
      return;
    }
  }
}

void AsyncTraceProfilerSink::DrainAndWrite(
    ::std::vector<ProfEvent>& batch) noexcept
{
  if (!m_Writer)
  {
    batch.clear();
    return;
  }

  for (const auto& ev : batch)
  {
    if (m_Time0Ns == 0)
      m_Time0Ns = ev.TimestampNs;

    EmitThreadNameOnce(ev.ThreadId, LookupThreadProfilerName(ev.ThreadId));

    if (!m_First)
      m_Writer->WriteString(",");
    m_First = false;

    WriteChromeTraceEvent(m_Writer.get(), ev, m_Time0Ns);
  }
  batch.clear();
}

void AsyncTraceProfilerSink::EmitThreadNameOnce(u32 tid,
                                                const char* name) noexcept
{
  if (!name)
    return;
  if (::std::find(m_NamedThreads.begin(), m_NamedThreads.end(), tid) !=
      m_NamedThreads.end())
    return;
  m_NamedThreads.push_back(tid);

  if (!m_First)
    m_Writer->WriteString(",");
  m_First = false;
  WriteChromeTraceThreadName(m_Writer.get(), tid, name);
}

}  // namespace gecko::runtime
