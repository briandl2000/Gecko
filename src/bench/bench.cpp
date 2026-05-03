/// @file
/// `gecko::bench` harness implementation.
///
/// Per-iteration metrics come from the profiler. The harness installs
/// an `IProfilerSink` that captures every ZoneBegin/ZoneEnd event, and
/// after the iteration loop ends, bins each matched zone by its begin
/// timestamp into the iteration window it belongs to. This means any
/// `GECKO_PROFILE_*` or `GECKO_GPU_PROF_SCOPE` opened inside an
/// iteration becomes a metric in the JSON output.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <gecko/bench/bench.h>
#include <gecko/core/labels.h>
#include <gecko/core/services/log.h>
#include <gecko/core/services/profiler.h>
#include <gecko/core/utility/time.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace gecko::bench {

namespace {

constexpr ::gecko::Label kBenchLabel = ::gecko::MakeLabel("gecko.bench");

struct ArgAxis
{
  ::std::string Name;
  ::std::vector<::gecko::i64> Values;
};

}  // namespace

class Case
{
public:
  ::std::string Name;
  ::std::string Description;
  CaseFn Fn {nullptr};
  ::gecko::u32 Iterations {100};
  ::gecko::u32 Warmup {5};
  ::std::vector<ArgAxis> Sweeps;
  View ViewHint {View::Auto};
};

namespace {

/// One zone matched from a ZoneBegin/ZoneEnd pair.
struct CapturedZone
{
  ::std::string Name;  ///< Stable copy of event name.
  ::gecko::ProfSource Source {::gecko::ProfSource::CPU};
  ::gecko::u64 BeginNs {0};  ///< Begin timestamp (used for iter binning).
  ::gecko::u64 DurationNs {0};
  ::gecko::u32 ThreadId {0};
};

/// Open zone tracked between ZoneBegin and ZoneEnd, keyed by
/// (ThreadId, Source). LIFO match.
struct OpenZone
{
  ::std::string Name;
  ::gecko::u32 ThreadId {0};
  ::gecko::ProfSource Source {::gecko::ProfSource::CPU};
  ::gecko::u64 BeginNs {0};
};

}  // namespace

struct State::Impl
{
  Case* Owner {nullptr};
  // Sweep args active for this point.
  ::std::map<::std::string, ::gecko::i64> ArgValues;

  // Iteration windows: for each measured iteration, the [start, end]
  // monotonic timestamp range. Used to bin profiler events.
  ::std::vector<::gecko::u64> IterStartNs;
  ::std::vector<::gecko::u64> IterEndNs;

  // Profiler-captured zones during the iteration loop. Filled by sink.
  ::std::vector<CapturedZone> Zones;

  // Stack of open zones, keyed by (ThreadId<<1 | source).
  ::std::map<::gecko::u64, ::std::vector<OpenZone>> OpenStacks;

  ::std::string AbortReason;
  bool Aborted {false};
  bool Started {false};
  ::gecko::u32 Index {0};       ///< Current iteration (incl. warmup).
  ::gecko::u32 TotalIters {0};  ///< Iterations + Warmup.
  ::gecko::u32 WarmupIters {0};
  ::gecko::u64 CurIterStartNs {0};

  // Profiler hookup -- installed lazily when the user enters the
  // for-loop (by which time the Engine/Profiler is up).
  ::gecko::IProfiler* Prof {nullptr};
  void* Sink {nullptr};  ///< type-erased BenchSink*
  ::gecko::ProfLevel SavedLevel {::gecko::ProfLevel::Normal};
  ::gecko::u32 SavedSampleRate {1};
  bool ProfInstalled {false};
};

namespace {

::std::vector<Case*>& Registry()
{
  static ::std::vector<Case*> r;
  return r;
}

/// Sink that captures ZoneBegin/ZoneEnd pairs into the active state.
class BenchSink final : public ::gecko::IProfilerSink
{
public:
  explicit BenchSink(State::Impl& impl) noexcept : m_Impl(impl)
  {}

  void Write(const ::gecko::ProfEvent& ev) noexcept override
  {
    if (ev.Kind != ::gecko::ProfEventKind::ZoneBegin &&
        ev.Kind != ::gecko::ProfEventKind::ZoneEnd)
      return;

    const ::gecko::u64 key = (static_cast<::gecko::u64>(ev.ThreadId) << 1) |
                             static_cast<::gecko::u64>(ev.Source);

    if (ev.Kind == ::gecko::ProfEventKind::ZoneBegin)
    {
      auto& stack = m_Impl.OpenStacks[key];
      OpenZone z;
      z.Name = ev.Name ? ev.Name : "";
      z.ThreadId = ev.ThreadId;
      z.Source = ev.Source;
      z.BeginNs = ev.TimestampNs;
      stack.push_back(::std::move(z));
    }
    else  // ZoneEnd
    {
      auto it = m_Impl.OpenStacks.find(key);
      if (it == m_Impl.OpenStacks.end() || it->second.empty())
        return;
      OpenZone z = ::std::move(it->second.back());
      it->second.pop_back();

      CapturedZone cz;
      cz.Name = ::std::move(z.Name);
      cz.Source = z.Source;
      cz.BeginNs = z.BeginNs;
      cz.DurationNs = ev.TimestampNs - z.BeginNs;
      cz.ThreadId = z.ThreadId;
      m_Impl.Zones.push_back(::std::move(cz));
    }
  }

  void WriteBatch(::gecko::Span<const ::gecko::ProfEvent> evs) noexcept override
  {
    for (const auto& ev : evs)
      Write(ev);
  }

  void Flush() noexcept override
  {}

private:
  State::Impl& m_Impl;
};

}  // namespace

// ---- Builder ---------------------------------------------------------------

Builder& Builder::Name(const char* name) noexcept
{
  m_Case->Name = name;
  return *this;
}
Builder& Builder::Iterations(::gecko::u32 n) noexcept
{
  m_Case->Iterations = n;
  return *this;
}
Builder& Builder::Warmup(::gecko::u32 n) noexcept
{
  m_Case->Warmup = n;
  return *this;
}
Builder& Builder::Description(const char* desc) noexcept
{
  m_Case->Description = desc;
  return *this;
}
Builder& Builder::Sweep(const char* name,
                        ::std::initializer_list<::gecko::i64> values) noexcept
{
  ArgAxis axis;
  axis.Name = name;
  axis.Values.assign(values.begin(), values.end());
  m_Case->Sweeps.push_back(::std::move(axis));
  return *this;
}
Builder& Builder::ViewHint(View v) noexcept
{
  m_Case->ViewHint = v;
  return *this;
}

Builder Register(const char* fnName, CaseFn fn) noexcept
{
  auto* c = new Case();
  c->Name = fnName;
  c->Fn = fn;
  Registry().push_back(c);
  return Builder(c);
}

// ---- State -----------------------------------------------------------------

::gecko::i64 State::Arg(const char* name) const noexcept
{
  if (!m_Impl)
    return 0;
  auto it = m_Impl->ArgValues.find(name);
  if (it == m_Impl->ArgValues.end())
  {
    GECKO_WARN(kBenchLabel, "State::Arg(\"{}\"): no such sweep axis", name);
    return 0;
  }
  return it->second;
}

void State::Abort(const char* reason) noexcept
{
  if (!m_Impl)
    return;
  m_Impl->Aborted = true;
  if (reason)
    m_Impl->AbortReason = reason;
}

::gecko::u32 State::Index() const noexcept
{
  if (!m_Impl)
    return 0;
  return m_Impl->Index >= m_Impl->WarmupIters
             ? m_Impl->Index - m_Impl->WarmupIters
             : 0;
}

::gecko::u32 State::Iterations() const noexcept
{
  return m_Impl ? (m_Impl->TotalIters - m_Impl->WarmupIters) : 0;
}

bool State::IsWarmup() const noexcept
{
  return m_Impl && m_Impl->Index < m_Impl->WarmupIters;
}

State::Iterator State::begin() noexcept
{
  if (m_Impl)
  {
    // Lazy profiler hookup: by the time we hit begin(), the user fn
    // has constructed any Engine/Module dependencies it needs.
    if (!m_Impl->ProfInstalled)
    {
      auto* prof = ::gecko::GetProfiler();
      if (prof)
      {
        m_Impl->Prof = prof;
        m_Impl->SavedLevel = prof->GetMinLevel();
        m_Impl->SavedSampleRate = prof->GetDetailedSampleRate();
        prof->SetMinLevel(::gecko::ProfLevel::Detailed);
        prof->SetDetailedSampleRate(1);
        prof->Flush();
        auto* sink = new BenchSink(*m_Impl);
        m_Impl->Sink = sink;
        prof->AddSink(sink);
        m_Impl->ProfInstalled = true;
      }
    }
    m_Impl->Started = true;
    m_Impl->Index = 0;
    m_Impl->CurIterStartNs = ::gecko::MonotonicTimeNs();
  }
  return Iterator {this};
}

State::Iterator State::end() noexcept
{
  return Iterator {nullptr};
}

State::Iterator& State::Iterator::operator++() noexcept
{
  if (!S || !S->m_Impl)
    return *this;
  auto* impl = S->m_Impl;
  const ::gecko::u64 endNs = ::gecko::MonotonicTimeNs();

  if (impl->Started && !impl->Aborted)
  {
    if (impl->Index >= impl->WarmupIters)
    {
      impl->IterStartNs.push_back(impl->CurIterStartNs);
      impl->IterEndNs.push_back(endNs);
    }
  }
  ++impl->Index;
  impl->CurIterStartNs = ::gecko::MonotonicTimeNs();
  return *this;
}

bool State::Iterator::operator!=(const Iterator& other) const noexcept
{
  if (!S)
    return false;  // we're 'end' sentinel
  if (!S->m_Impl)
    return false;
  auto* impl = S->m_Impl;
  const bool more =
      !impl->Aborted && impl->Index < impl->TotalIters && other.S == nullptr;
  if (!more && impl->ProfInstalled && impl->Prof)
  {
    // Loop is exiting: drain & detach sink while the profiler/engine
    // are still alive (the fixture goes out of scope right after).
    impl->Prof->Flush();
    auto* sink = static_cast<BenchSink*>(impl->Sink);
    impl->Prof->RemoveSink(sink);
    impl->Prof->SetMinLevel(impl->SavedLevel);
    impl->Prof->SetDetailedSampleRate(impl->SavedSampleRate);
    delete sink;
    impl->Sink = nullptr;
    impl->Prof = nullptr;
    impl->ProfInstalled = false;  // already torn down
  }
  return more;
}

// ---- Stats / runner --------------------------------------------------------

namespace {

struct Stats
{
  ::gecko::u64 Min {0};
  ::gecko::u64 Max {0};
  ::gecko::u64 Mean {0};
  ::gecko::u64 P50 {0};
  ::gecko::u64 P95 {0};
  double Stddev {0.0};
};

Stats ComputeStats(const ::std::vector<::gecko::u64>& samples)
{
  Stats s;
  if (samples.empty())
    return s;
  ::std::vector<::gecko::u64> sorted = samples;
  ::std::sort(sorted.begin(), sorted.end());
  s.Min = sorted.front();
  s.Max = sorted.back();
  ::gecko::u64 sum = 0;
  for (auto v : sorted)
    sum += v;
  s.Mean = sum / sorted.size();
  s.P50 = sorted[sorted.size() / 2];
  s.P95 = sorted[(sorted.size() * 95) / 100];
  double meanD = static_cast<double>(s.Mean);
  double acc = 0.0;
  for (auto v : sorted)
  {
    double d = static_cast<double>(v) - meanD;
    acc += d * d;
  }
  s.Stddev = ::std::sqrt(acc / static_cast<double>(sorted.size()));
  return s;
}

::std::string EscapeJson(const ::std::string& in)
{
  ::std::string out;
  out.reserve(in.size() + 2);
  for (char c : in)
  {
    switch (c)
    {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

::std::string IsoNow()
{
  ::std::time_t t = ::std::time(nullptr);
  ::std::tm tm {};
#if defined(_WIN32)
  ::gmtime_s(&tm, &t);
#else
  ::gmtime_r(&t, &tm);
#endif
  char buf[32];
  ::std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

::std::string PlatformId()
{
#if defined(GECKO_PLATFORM_WINDOWS)
  return "Windows";
#elif defined(GECKO_PLATFORM_LINUX)
  return "Linux";
#else
  return "Unknown";
#endif
}

::std::string BuildConfig()
{
#ifdef NDEBUG
  return "Release";
#else
  return "Debug";
#endif
}

const char* SourceStr(::gecko::ProfSource s)
{
  return s == ::gecko::ProfSource::GPU ? "gpu" : "cpu";
}

const char* ViewStr(View v, bool hasSweep)
{
  switch (v)
  {
  case View::Lines:
    return "lines";
  case View::Bars:
    return "bars";
  case View::Auto:
  default:
    return hasSweep ? "bars" : "lines";
  }
}

/// Bin captured zones by iteration. Each iter gets a map of
/// metric name -> list of durations summed per iter (multiple events
/// with same name in one iter are summed).
struct PerIterMetrics
{
  // metricKey = "name|source"; samples[iter] = total ns for that metric
  // in that iter (0 if no event).
  ::std::map<::std::string,
             ::std::pair<::gecko::ProfSource, ::std::vector<::gecko::u64>>>
      Metrics;
};

PerIterMetrics BinZones(const State::Impl& impl)
{
  PerIterMetrics out;
  const size_t N = impl.IterStartNs.size();
  if (N == 0)
    return out;

  // Find the iter index a timestamp belongs to via binary search on
  // IterStartNs. The window is [IterStartNs[i], IterEndNs[i]]. For
  // GPU events whose Begin TS may slip slightly into the next iter,
  // we still attribute by Begin -- close enough.
  for (const auto& z : impl.Zones)
  {
    // find iter window that contains z.BeginNs
    auto it = ::std::upper_bound(impl.IterStartNs.begin(),
                                 impl.IterStartNs.end(), z.BeginNs);
    if (it == impl.IterStartNs.begin())
      continue;
    size_t i = static_cast<size_t>(it - impl.IterStartNs.begin() - 1);
    if (i >= N)
      continue;
    if (z.BeginNs > impl.IterEndNs[i])
    {
      // GPU event resolved after the iter ended but before the next
      // iter started -- still attribute to iter i.
      // No-op; fall through.
    }
    auto& entry = out.Metrics[z.Name];
    entry.first = z.Source;
    if (entry.second.size() < N)
      entry.second.resize(N, 0);
    entry.second[i] += z.DurationNs;
  }
  return out;
}

void EmitMetric(::std::ostream& json, bool& first, const ::std::string& name,
                ::gecko::ProfSource source,
                const ::std::vector<::gecko::u64>& samples)
{
  if (!first)
    json << ",";
  first = false;
  Stats st = ComputeStats(samples);
  json << "\n        \"" << EscapeJson(name) << "\": {";
  json << "\"source\": \"" << SourceStr(source) << "\"";
  json << ", \"stats_ns\": {";
  json << "\"min\": " << st.Min << ", \"max\": " << st.Max
       << ", \"mean\": " << st.Mean << ", \"p50\": " << st.P50
       << ", \"p95\": " << st.P95
       << ", \"stddev\": " << static_cast<::gecko::u64>(st.Stddev) << "}";
  json << ", \"samples_ns\": [";
  for (size_t i = 0; i < samples.size(); ++i)
  {
    if (i)
      json << ",";
    json << samples[i];
  }
  json << "]}";
}

void RunOneInvocation(Case& c,
                      const ::std::map<::std::string, ::gecko::i64>& args,
                      ::std::ostream& json, bool& firstInvocation)
{
  State::Impl impl;
  impl.Owner = &c;
  impl.ArgValues = args;
  impl.TotalIters = c.Iterations + c.Warmup;
  impl.WarmupIters = c.Warmup;

  State s;
  s.m_Impl = &impl;

  GECKO_INFO(kBenchLabel, "  running '{}' ({} warmup + {} iter)", c.Name,
             c.Warmup, c.Iterations);

  c.Fn(s);

  // Belt-and-suspenders: if the loop never ran (no iterations), tear
  // the sink down here. Normal path: cleanup already happened inside
  // the iterator when the for-loop exited.
  if (impl.ProfInstalled && impl.Prof)
  {
    impl.Prof->Flush();
    auto* sink = static_cast<BenchSink*>(impl.Sink);
    impl.Prof->RemoveSink(sink);
    impl.Prof->SetMinLevel(impl.SavedLevel);
    impl.Prof->SetDetailedSampleRate(impl.SavedSampleRate);
    delete sink;
    impl.Sink = nullptr;
  }

  // Build per-iter metrics. Always emit synthetic 'frame_total' from
  // the harness's own timing (CPU).
  PerIterMetrics metrics = BinZones(impl);
  ::std::vector<::gecko::u64> frameTotal;
  frameTotal.reserve(impl.IterStartNs.size());
  for (size_t i = 0; i < impl.IterStartNs.size(); ++i)
    frameTotal.push_back(impl.IterEndNs[i] - impl.IterStartNs[i]);

  if (!firstInvocation)
    json << ",";
  firstInvocation = false;

  json << "\n    {";
  json << "\n      \"name\": \"" << EscapeJson(c.Name) << "\"";
  if (!c.Description.empty())
    json << ",\n      \"description\": \"" << EscapeJson(c.Description) << "\"";

  // Args (sweep values)
  json << ",\n      \"args\": {";
  bool firstA = true;
  for (auto& [k, v] : impl.ArgValues)
  {
    if (!firstA)
      json << ", ";
    firstA = false;
    json << "\"" << EscapeJson(k) << "\": " << v;
  }
  json << "}";

  json << ",\n      \"iterations\": " << impl.IterStartNs.size();
  json << ",\n      \"view\": \"" << ViewStr(c.ViewHint, !c.Sweeps.empty())
       << "\"";

  // Metrics map.
  json << ",\n      \"metrics\": {";
  bool firstM = true;
  EmitMetric(json, firstM, "frame_total", ::gecko::ProfSource::CPU, frameTotal);
  for (auto& [name, entry] : metrics.Metrics)
  {
    EmitMetric(json, firstM, name, entry.first, entry.second);
  }
  json << "\n      }";

  if (!impl.AbortReason.empty())
    json << ",\n      \"aborted\": \"" << EscapeJson(impl.AbortReason) << "\"";

  json << "\n    }";
}

void RunCase(Case& c, ::std::ostream& json, bool& firstInvocation)
{
  if (c.Sweeps.empty())
  {
    RunOneInvocation(c, {}, json, firstInvocation);
    return;
  }
  ::std::vector<size_t> idx(c.Sweeps.size(), 0);
  for (;;)
  {
    ::std::map<::std::string, ::gecko::i64> args;
    for (size_t a = 0; a < c.Sweeps.size(); ++a)
      args[c.Sweeps[a].Name] = c.Sweeps[a].Values[idx[a]];
    RunOneInvocation(c, args, json, firstInvocation);

    size_t a = 0;
    for (; a < c.Sweeps.size(); ++a)
    {
      if (++idx[a] < c.Sweeps[a].Values.size())
        break;
      idx[a] = 0;
    }
    if (a == c.Sweeps.size())
      break;
  }
}

void PrintHelp(const char* prog)
{
  ::std::printf(
      "Usage: %s [options]\n"
      "  --case <name>   Run only the case with this name (default: all)\n"
      "  --list          List registered cases and exit\n"
      "  --out <path>    Write JSON results to <path>\n"
      "  --iters <n>     Override measured iteration count for all cases\n"
      "  --warmup <n>    Override warmup iteration count for all cases\n"
      "  --program <id>  Program identifier in JSON meta (default: exe "
      "basename)\n"
      "  --git <hash>    Git hash recorded in JSON meta\n"
      "  --help          Show this help and exit\n",
      prog);
}

}  // namespace

int Main(int argc, char** argv) noexcept
{
  const char* filter = nullptr;
  const char* outPath = nullptr;
  const char* programId = nullptr;
  const char* gitHash = "";
  ::gecko::i32 iterOverride = -1;
  ::gecko::i32 warmupOverride = -1;
  bool list = false;

  for (int i = 1; i < argc; ++i)
  {
    const char* a = argv[i];
    auto take = [&](const char* flag) -> const char* {
      if (::std::strcmp(a, flag) == 0 && i + 1 < argc)
        return argv[++i];
      return nullptr;
    };
    if (auto v = take("--case"))
      filter = v;
    else if (auto v = take("--out"))
      outPath = v;
    else if (auto v = take("--program"))
      programId = v;
    else if (auto v = take("--git"))
      gitHash = v;
    else if (auto v = take("--iters"))
      iterOverride = ::std::atoi(v);
    else if (auto v = take("--warmup"))
      warmupOverride = ::std::atoi(v);
    else if (::std::strcmp(a, "--list") == 0)
      list = true;
    else if (::std::strcmp(a, "--help") == 0 || ::std::strcmp(a, "-h") == 0)
    {
      PrintHelp(argv[0]);
      return 0;
    }
    else
    {
      ::std::fprintf(stderr, "unknown arg: %s\n", a);
      PrintHelp(argv[0]);
      return 2;
    }
  }

  if (list)
  {
    for (auto* c : Registry())
      ::std::printf("%s\n", c->Name.c_str());
    return 0;
  }

  ::std::string defaultProg;
  if (!programId)
  {
    ::std::string exe = argv[0] ? argv[0] : "bench";
    auto slash = exe.find_last_of("/\\");
    defaultProg = (slash == ::std::string::npos) ? exe : exe.substr(slash + 1);
    if (defaultProg.size() >= 4 &&
        defaultProg.compare(defaultProg.size() - 4, 4, ".exe") == 0)
      defaultProg.resize(defaultProg.size() - 4);
    programId = defaultProg.c_str();
  }

  ::std::ostringstream json;
  json << "{";
  json << "\n  \"meta\": {";
  json << "\n    \"program\": \"" << EscapeJson(programId) << "\"";
  json << ",\n    \"timestamp\": \"" << IsoNow() << "\"";
  json << ",\n    \"platform\": \"" << PlatformId() << "\"";
  json << ",\n    \"build_config\": \"" << BuildConfig() << "\"";
  json << ",\n    \"git\": \"" << EscapeJson(gitHash) << "\"";
  json << "\n  },";
  json << "\n  \"cases\": [";

  bool first = true;
  bool any = false;
  for (auto* c : Registry())
  {
    if (filter && c->Name != filter)
      continue;
    any = true;
    if (iterOverride > 0)
      c->Iterations = static_cast<::gecko::u32>(iterOverride);
    if (warmupOverride >= 0)
      c->Warmup = static_cast<::gecko::u32>(warmupOverride);
    RunCase(*c, json, first);
  }

  if (!any && filter)
  {
    GECKO_ERROR(kBenchLabel, "No case matched filter '{}'", filter);
    return 1;
  }

  json << "\n  ]";
  json << "\n}\n";

  ::std::string js = json.str();
  ::std::fputs(js.c_str(), stdout);
  ::std::fflush(stdout);

  if (outPath)
  {
    ::std::filesystem::path p(outPath);
    if (p.has_parent_path())
      ::std::filesystem::create_directories(p.parent_path());
    ::std::ofstream f(p, ::std::ios::out | ::std::ios::trunc);
    if (!f)
    {
      GECKO_ERROR(kBenchLabel, "Failed to open output: {}", outPath);
      return 1;
    }
    f << js;
    GECKO_INFO(kBenchLabel, "wrote {}", outPath);
  }

  return 0;
}

}  // namespace gecko::bench
