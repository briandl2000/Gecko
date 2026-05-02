/// @file
/// `gecko::bench` harness implementation.

#include <algorithm>
#include <chrono>
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
#include <gecko/core/utility/time.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace gecko::bench {

namespace {

constexpr ::gecko::Label kBenchLabel = ::gecko::MakeLabel("gecko.bench");

struct SectionAgg
{
  ::gecko::u64 TotalNs {0};
  ::gecko::u32 Count {0};
};

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
};

struct State::Impl
{
  Case* Owner {nullptr};
  ::std::vector<::gecko::u64> SampleNs;  ///< Per-iteration totals.
  ::std::map<::std::string, SectionAgg> Sections;
  ::std::map<::std::string, ::gecko::i64> Counters;
  ::std::map<::std::string, ::gecko::i64> ArgValues;
  ::std::string AbortReason;
};

namespace {

::std::vector<Case*>& Registry()
{
  static ::std::vector<Case*> r;
  return r;
}

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

Builder Register(const char* fnName, CaseFn fn) noexcept
{
  auto* c = new Case();
  c->Name = fnName;
  c->Fn = fn;
  Registry().push_back(c);
  return Builder(c);
}

// ---- State -----------------------------------------------------------------

State::Iterator State::begin() noexcept
{
  StartFirstIter();
  return Iterator {this};
}
State::Iterator State::end() noexcept
{
  return Iterator {nullptr};
}

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
  m_Aborted = true;
  if (m_Impl && reason)
    m_Impl->AbortReason = reason;
}

void State::Counter(const char* name, ::gecko::i64 value) noexcept
{
  if (m_Impl && name)
    m_Impl->Counters[name] = value;
}

void State::StartFirstIter() noexcept
{
  m_Started = true;
  m_Index = 0;
  m_IterStartNs = ::gecko::HighResTimeNs();
}

void State::NextIter() noexcept
{
  const ::gecko::u64 endNs = ::gecko::HighResTimeNs();
  if (m_Started && m_Impl && !m_Aborted)
  {
    if (m_Index >= m_Warmup)
      m_Impl->SampleNs.push_back(endNs - m_IterStartNs);
  }
  ++m_Index;
  m_IterStartNs = ::gecko::HighResTimeNs();
}

State::ScopedSection::ScopedSection(State& s, const char* name) noexcept
    : m_State(s), m_Name(name), m_StartNs(::gecko::HighResTimeNs())
{}
State::ScopedSection::~ScopedSection() noexcept
{
  if (!m_State.m_Impl || !m_Name)
    return;
  // Skip warmup iterations.
  if (m_State.m_Index < m_State.m_Warmup)
    return;
  const ::gecko::u64 endNs = ::gecko::HighResTimeNs();
  auto& agg = m_State.m_Impl->Sections[m_Name];
  agg.TotalNs += (endNs - m_StartNs);
  agg.Count += 1;
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
  // Stddev:
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

void RunOneInvocation(Case& c,
                      const ::std::map<::std::string, ::gecko::i64>& args,
                      ::std::ostream& json, bool& firstInvocation)
{
  State::Impl impl;
  impl.Owner = &c;
  impl.ArgValues = args;

  State s;
  s.m_Impl = &impl;
  s.m_Total = c.Iterations + c.Warmup;
  s.m_Warmup = c.Warmup;

  GECKO_INFO(kBenchLabel, "  running '{}' ({} warmup + {} iter)", c.Name,
             c.Warmup, c.Iterations);

  c.Fn(s);

  // If the user used the for-range, NextIter() runs after each iteration
  // and the last sample is captured. If the user didn't use the for-range
  // at all, sample list will be empty -- record that as an aborted case.

  Stats st = ComputeStats(impl.SampleNs);

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

  // Stats (ns)
  json << ",\n      \"iterations\": " << impl.SampleNs.size();
  json << ",\n      \"stats_ns\": {";
  json << "\"min\": " << st.Min << ", \"max\": " << st.Max;
  json << ", \"mean\": " << st.Mean << ", \"p50\": " << st.P50;
  json << ", \"p95\": " << st.P95
       << ", \"stddev\": " << static_cast<::gecko::u64>(st.Stddev);
  json << "}";

  // Sections
  json << ",\n      \"sections\": {";
  bool firstS = true;
  for (auto& [k, agg] : impl.Sections)
  {
    if (!firstS)
      json << ", ";
    firstS = false;
    ::gecko::u64 mean = agg.Count ? (agg.TotalNs / agg.Count) : 0;
    json << "\"" << EscapeJson(k) << "\": {";
    json << "\"count\": " << agg.Count;
    json << ", \"total_ns\": " << agg.TotalNs;
    json << ", \"mean_ns\": " << mean;
    json << "}";
  }
  json << "}";

  // Counters
  json << ",\n      \"counters\": {";
  bool firstC = true;
  for (auto& [k, v] : impl.Counters)
  {
    if (!firstC)
      json << ", ";
    firstC = false;
    json << "\"" << EscapeJson(k) << "\": " << v;
  }
  json << "}";

  // Samples (raw, optional but useful for graphs)
  json << ",\n      \"samples_ns\": [";
  for (size_t i = 0; i < impl.SampleNs.size(); ++i)
  {
    if (i)
      json << ",";
    json << impl.SampleNs[i];
  }
  json << "]";

  if (!impl.AbortReason.empty())
  {
    json << ",\n      \"aborted\": \"" << EscapeJson(impl.AbortReason) << "\"";
  }

  json << "\n    }";
}

void RunCase(Case& c, ::std::ostream& json, bool& firstInvocation)
{
  if (c.Sweeps.empty())
  {
    RunOneInvocation(c, {}, json, firstInvocation);
    return;
  }
  // Cartesian product of sweep axes.
  ::std::vector<size_t> idx(c.Sweeps.size(), 0);
  for (;;)
  {
    ::std::map<::std::string, ::gecko::i64> args;
    for (size_t a = 0; a < c.Sweeps.size(); ++a)
      args[c.Sweeps[a].Name] = c.Sweeps[a].Values[idx[a]];
    RunOneInvocation(c, args, json, firstInvocation);

    // Advance.
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
      "  --out <path>    Write JSON results to <path> (default: stdout only)\n"
      "  --iters <n>     Override measured iteration count for all cases\n"
      "  --warmup <n>    Override warmup iteration count for all cases\n"
      "  --program <id>  Program identifier recorded in JSON meta (default: "
      "exe basename)\n"
      "  --git <hash>    Git hash recorded in JSON meta (default: \"\")\n"
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

  // Default program id = exe basename.
  ::std::string defaultProg;
  if (!programId)
  {
    ::std::string exe = argv[0] ? argv[0] : "bench";
    auto slash = exe.find_last_of("/\\");
    defaultProg = (slash == ::std::string::npos) ? exe : exe.substr(slash + 1);
    // Strip .exe if present.
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
