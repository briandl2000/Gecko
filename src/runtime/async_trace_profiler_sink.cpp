#include "gecko/runtime/async_trace_profiler_sink.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/platform/platform_io.h"
#include "private/chrome_trace_format.h"
#include "private/file_writer_format.h"
#include "private/labels.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

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

  bool Write(::gecko::ConstByteSpan data) noexcept override
  {
    m_Buf.append(reinterpret_cast<const char*>(data.Data()), data.Count());
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

struct AsyncTraceProfilerSink::Impl
{
  ::gecko::Unique<::gecko::platform::FileWriter> Writer {};
  bool First {true};
  u64 Time0Ns {0};

  ::std::mutex Mu {};
  ::std::condition_variable Cv {};
  ::std::vector<ProfEvent> Pending {};
  ::std::atomic<bool> Run {true};
  ::std::thread Worker {};
  ::std::atomic<ProfLevel> MinLevel {ProfLevel::Detailed};

  // Serialises every write to Writer (worker drain, Flush(), destructor).
  // Without this, concurrent drains produced double-comma corruption.
  ::std::mutex WriteMu {};

  // Already-emitted thread_name metadata (TID -> done). Worker-only.
  ::std::vector<u32> NamedThreads {};
};

template <class ImplT>
void DrainAndWrite(ImplT& impl, ::std::vector<ProfEvent>& batch) noexcept
{
  if (!impl.Writer)
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
    if (impl.Time0Ns == 0)
      impl.Time0Ns = ev.TimestampNs;

    if (const char* tname = LookupThreadProfilerName(ev.ThreadId); tname != nullptr)
    {
      if (::std::find(impl.NamedThreads.begin(), impl.NamedThreads.end(), ev.ThreadId) == impl.NamedThreads.end())
      {
        impl.NamedThreads.push_back(ev.ThreadId);
        if (!impl.First)
          buf.Buffer().push_back(',');
        impl.First = false;
        WriteChromeTraceThreadName(&buf, ev.ThreadId, tname);
      }
    }

    if (!impl.First)
      buf.Buffer().push_back(',');
    impl.First = false;
    WriteChromeTraceEvent(&buf, ev, impl.Time0Ns);
  }

  if (!buf.Buffer().empty())
    impl.Writer->WriteString(::gecko::StringView {buf.Buffer().data(), buf.Buffer().size()});

  batch.clear();
}

AsyncTraceProfilerSink::AsyncTraceProfilerSink(const char* path)
{
  m_Impl.reset(new (::std::nothrow) Impl());
  if (!m_Impl)
    return;

  // A null/empty path is the explicit "disabled" mode: the sink object can
  // still be constructed and registered, but it produces no file and the
  // worker thread is not spawned. IsOpen() reports false in that case.
  if (!path || path[0] == '\0')
    return;

  m_Impl->Writer = ::gecko::platform::OpenWrite(path, ::gecko::platform::WriteMode::Truncate);

  if (!m_Impl->Writer)
    return;

  m_Impl->Writer->WriteString("{\"traceEvents\":[");
  m_Impl->Writer->Flush();

  m_Impl->Worker = ::std::thread([this]() { WorkerLoop(); });
}

AsyncTraceProfilerSink::~AsyncTraceProfilerSink()
{
  // Unregister BEFORE we tear anything down: the base ~RegisteredSink runs
  // after this body, so without this the profiler would forward final
  // events into m_Pending after we already closed the writer and they'd
  // be silently dropped (the trace looked like the main scope never
  // finished).
  Unregister();

  if (!m_Impl)
    return;

  m_Impl->Run.store(false, ::std::memory_order_release);
  m_Impl->Cv.notify_all();
  if (m_Impl->Worker.joinable())
    m_Impl->Worker.join();

  ::std::lock_guard<::std::mutex> wlk(m_Impl->WriteMu);
  if (m_Impl->Writer)
  {
    // Drain anything that arrived after the worker exited.
    if (!m_Impl->Pending.empty())
    {
      ::std::vector<ProfEvent> batch;
      {
        ::std::lock_guard<::std::mutex> lk(m_Impl->Mu);
        batch.swap(m_Impl->Pending);
      }
      DrainAndWrite(*m_Impl, batch);
    }
    m_Impl->Writer->WriteString("]}");
    m_Impl->Writer->Flush();
    m_Impl->Writer.reset();
  }
}

bool AsyncTraceProfilerSink::IsOpen() const noexcept
{
  return m_Impl && m_Impl->Writer;
}

void AsyncTraceProfilerSink::SetMinLevel(ProfLevel level) noexcept
{
  if (m_Impl)
    m_Impl->MinLevel.store(level, ::std::memory_order_relaxed);
}

ProfLevel AsyncTraceProfilerSink::GetMinLevel() const noexcept
{
  return m_Impl ? m_Impl->MinLevel.load(::std::memory_order_relaxed) : ProfLevel::Detailed;
}

void AsyncTraceProfilerSink::Write(const ProfEvent& event) noexcept
{
  if (!m_Impl || !m_Impl->Writer)
    return;
  // Always allow Counter / Mark events through regardless of min-level --
  // these are typically per-frame summaries the user explicitly opted into.
  if (event.Kind == ProfEventKind::ZoneBegin || event.Kind == ProfEventKind::ZoneEnd)
  {
    if (event.Level > m_Impl->MinLevel.load(::std::memory_order_relaxed))
      return;
  }
  {
    ::std::lock_guard<::std::mutex> lk(m_Impl->Mu);
    m_Impl->Pending.push_back(event);
  }
  m_Impl->Cv.notify_one();
}

void AsyncTraceProfilerSink::WriteBatch(::gecko::Span<const ProfEvent> events) noexcept
{
  if (!m_Impl || !m_Impl->Writer || events.empty())
    return;
  const ProfLevel minLevel = m_Impl->MinLevel.load(::std::memory_order_relaxed);
  {
    ::std::lock_guard<::std::mutex> lk(m_Impl->Mu);
    m_Impl->Pending.reserve(m_Impl->Pending.size() + events.size());
    for (const ProfEvent& e : events)
    {
      const bool isZone = (e.Kind == ProfEventKind::ZoneBegin || e.Kind == ProfEventKind::ZoneEnd);
      if (isZone && e.Level > minLevel)
        continue;
      m_Impl->Pending.push_back(e);
    }
  }
  m_Impl->Cv.notify_one();
}

void AsyncTraceProfilerSink::Flush() noexcept
{
  if (!m_Impl || !m_Impl->Writer)
    return;

  // Snapshot pending events under the queue mutex, then format under the
  // writer mutex so we can't interleave bytes with the worker thread.
  ::std::vector<ProfEvent> batch;
  {
    ::std::lock_guard<::std::mutex> lk(m_Impl->Mu);
    batch.swap(m_Impl->Pending);
  }
  ::std::lock_guard<::std::mutex> wlk(m_Impl->WriteMu);
  DrainAndWrite(*m_Impl, batch);
  if (m_Impl->Writer)
    m_Impl->Writer->Flush();
}

void AsyncTraceProfilerSink::WorkerLoop() noexcept
{
  if (!m_Impl)
    return;

  ::std::vector<ProfEvent> batch;
  auto lastFsync = ::std::chrono::steady_clock::now();

  while (true)
  {
    {
      ::std::unique_lock<::std::mutex> lk(m_Impl->Mu);
      m_Impl->Cv.wait_for(lk, c_DrainTickInterval, [this]() {
        return !m_Impl->Run.load(::std::memory_order_acquire) || !m_Impl->Pending.empty();
      });
      batch.swap(m_Impl->Pending);
    }

    if (!batch.empty())
    {
      GECKO_PROFILE_NAMED(labels::Profiler, "TraceSink::DrainAndWrite");
      ::std::lock_guard<::std::mutex> wlk(m_Impl->WriteMu);
      DrainAndWrite(*m_Impl, batch);
    }

    auto now = ::std::chrono::steady_clock::now();
    if (now - lastFsync >= c_FsyncInterval)
    {
      GECKO_PROFILE_NAMED(labels::Profiler, "TraceSink::Fsync");
      ::std::lock_guard<::std::mutex> wlk(m_Impl->WriteMu);
      if (m_Impl->Writer)
        m_Impl->Writer->Flush();
      lastFsync = now;
    }

    if (!m_Impl->Run.load(::std::memory_order_acquire))
    {
      // Final drain for events queued between the wait and the flag.
      {
        ::std::lock_guard<::std::mutex> lk(m_Impl->Mu);
        batch.swap(m_Impl->Pending);
      }
      ::std::lock_guard<::std::mutex> wlk(m_Impl->WriteMu);
      if (!batch.empty())
        DrainAndWrite(*m_Impl, batch);
      if (m_Impl->Writer)
        m_Impl->Writer->Flush();
      return;
    }
  }
}

}  // namespace gecko::runtime
