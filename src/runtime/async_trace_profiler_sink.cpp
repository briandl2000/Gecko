#include "gecko/runtime/async_trace_profiler_sink.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/platform/platform_io.h"
#include "private/chrome_trace_format.h"
#include "private/file_writer_format.h"
#include "private/labels.h"

#include <algorithm>
#include <chrono>

namespace gecko::runtime {

namespace {

using ::gecko::runtime::detail::WriteChromeTraceEvent;
using ::gecko::runtime::detail::WriteChromeTraceThreadName;

constexpr ::std::chrono::milliseconds c_FsyncInterval {1000};
constexpr ::std::chrono::milliseconds c_DrainTickInterval {25};

// In-memory FileWriter that just appends to a std::string. Used by
// DrainAndWrite to coalesce a whole event batch into one WriteFile call,
// which is critical when the trace file lives on a network share (each
// individual WriteFile becomes an SMB round-trip).
class StringBufferWriter final : public ::gecko::platform::FileWriter
{
public:
  ::std::string& Buffer() noexcept
  {
    return m_Buf;
  }

  bool Write(::std::span<const ::std::byte> data) noexcept override
  {
    m_Buf.append(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
  }
  bool Flush() noexcept override
  {
    return true;
  }
  ::gecko::u64 Seek(::gecko::i64, bool) noexcept override
  {
    return static_cast<::gecko::u64>(-1);
  }
  ::gecko::u64 Tell() noexcept override
  {
    return static_cast<::gecko::u64>(m_Buf.size());
  }

private:
  ::std::string m_Buf;
};

}  // namespace

AsyncTraceProfilerSink::AsyncTraceProfilerSink(const char* path)
{
  // A null/empty path is the explicit "disabled" mode: the sink object can
  // still be constructed and registered, but it produces no file and the
  // worker thread is not spawned. IsOpen() reports false in that case.
  if (!path || path[0] == '\0')
    return;

  m_Writer = ::gecko::platform::OpenWrite(path, ::gecko::platform::WriteMode::Truncate);

  if (!m_Writer)
    return;

  m_Writer->WriteString("{\"traceEvents\":[");
  m_Writer->Flush();

  m_Worker = ::std::thread([this]() { WorkerLoop(); });
}

AsyncTraceProfilerSink::~AsyncTraceProfilerSink()
{
  // Unregister BEFORE we tear anything down: the base ~RegisteredSink runs
  // after this body, so without this the profiler would forward final
  // events into m_Pending after we already closed the writer and they'd
  // be silently dropped (the trace looked like the main scope never
  // finished).
  Unregister();

  m_Run.store(false, ::std::memory_order_release);
  m_Cv.notify_all();
  if (m_Worker.joinable())
    m_Worker.join();

  ::std::lock_guard<::std::mutex> wlk(m_WriteMu);
  if (m_Writer)
  {
    // Drain anything that arrived after the worker exited.
    if (!m_Pending.empty())
    {
      ::std::vector<ProfEvent> batch;
      {
        ::std::lock_guard<::std::mutex> lk(m_Mu);
        batch.swap(m_Pending);
      }
      DrainAndWrite(batch);
    }
    m_Writer->WriteString("]}");
    m_Writer->Flush();
    m_Writer.reset();
  }
}

void AsyncTraceProfilerSink::Write(const ProfEvent& event) noexcept
{
  if (!m_Writer)
    return;
  // Always allow Counter / Mark events through regardless of min-level --
  // these are typically per-frame summaries the user explicitly opted into.
  if (event.Kind == ProfEventKind::ZoneBegin || event.Kind == ProfEventKind::ZoneEnd)
  {
    if (event.Level > m_MinLevel.load(::std::memory_order_relaxed))
      return;
  }
  {
    ::std::lock_guard<::std::mutex> lk(m_Mu);
    m_Pending.push_back(event);
  }
  m_Cv.notify_one();
}

void AsyncTraceProfilerSink::WriteBatch(::gecko::Span<const ProfEvent> events) noexcept
{
  if (!m_Writer || events.empty())
    return;
  const ProfLevel minLevel = m_MinLevel.load(::std::memory_order_relaxed);
  {
    ::std::lock_guard<::std::mutex> lk(m_Mu);
    m_Pending.reserve(m_Pending.size() + events.size());
    for (const ProfEvent& e : events)
    {
      const bool isZone = (e.Kind == ProfEventKind::ZoneBegin || e.Kind == ProfEventKind::ZoneEnd);
      if (isZone && e.Level > minLevel)
        continue;
      m_Pending.push_back(e);
    }
  }
  m_Cv.notify_one();
}

void AsyncTraceProfilerSink::Flush() noexcept
{
  if (!m_Writer)
    return;

  // Snapshot pending events under the queue mutex, then format under the
  // writer mutex so we can't interleave bytes with the worker thread.
  ::std::vector<ProfEvent> batch;
  {
    ::std::lock_guard<::std::mutex> lk(m_Mu);
    batch.swap(m_Pending);
  }
  ::std::lock_guard<::std::mutex> wlk(m_WriteMu);
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
      m_Cv.wait_for(lk, c_DrainTickInterval,
                    [this]() { return !m_Run.load(::std::memory_order_acquire) || !m_Pending.empty(); });
      batch.swap(m_Pending);
    }

    if (!batch.empty())
    {
      GECKO_PROFILE_NAMED(labels::Profiler, "TraceSink::DrainAndWrite");
      ::std::lock_guard<::std::mutex> wlk(m_WriteMu);
      DrainAndWrite(batch);
    }

    auto now = ::std::chrono::steady_clock::now();
    if (now - lastFsync >= c_FsyncInterval)
    {
      GECKO_PROFILE_NAMED(labels::Profiler, "TraceSink::Fsync");
      ::std::lock_guard<::std::mutex> wlk(m_WriteMu);
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
      ::std::lock_guard<::std::mutex> wlk(m_WriteMu);
      if (!batch.empty())
        DrainAndWrite(batch);
      if (m_Writer)
        m_Writer->Flush();
      return;
    }
  }
}

void AsyncTraceProfilerSink::DrainAndWrite(::std::vector<ProfEvent>& batch) noexcept
{
  if (!m_Writer)
  {
    batch.clear();
    return;
  }

  // Accumulate the entire batch into one in-memory string, then issue
  // a single WriteFile call to the real writer. This collapses what
  // would otherwise be ~2N WriteFile/SMB round-trips into 1.
  StringBufferWriter buf;
  for (const auto& ev : batch)
  {
    if (m_Time0Ns == 0)
      m_Time0Ns = ev.TimestampNs;

    if (const char* tname = LookupThreadProfilerName(ev.ThreadId); tname != nullptr)
    {
      if (::std::find(m_NamedThreads.begin(), m_NamedThreads.end(), ev.ThreadId) == m_NamedThreads.end())
      {
        m_NamedThreads.push_back(ev.ThreadId);
        if (!m_First)
          buf.Buffer().push_back(',');
        m_First = false;
        WriteChromeTraceThreadName(&buf, ev.ThreadId, tname);
      }
    }

    if (!m_First)
      buf.Buffer().push_back(',');
    m_First = false;
    WriteChromeTraceEvent(&buf, ev, m_Time0Ns);
  }

  if (!buf.Buffer().empty())
    m_Writer->WriteString(buf.Buffer());

  batch.clear();
}

void AsyncTraceProfilerSink::EmitThreadNameOnce(u32 tid, const char* name) noexcept
{
  if (!name)
    return;
  if (::std::find(m_NamedThreads.begin(), m_NamedThreads.end(), tid) != m_NamedThreads.end())
    return;
  m_NamedThreads.push_back(tid);

  if (!m_First)
    m_Writer->WriteString(",");
  m_First = false;
  WriteChromeTraceThreadName(m_Writer.get(), tid, name);
}

}  // namespace gecko::runtime
