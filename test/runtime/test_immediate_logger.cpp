#include "gecko/core/labels.h"
#include "gecko/runtime/immediate_logger.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>
#include <vector>

using namespace gecko;
using namespace gecko::runtime;

namespace {

struct CapturedMessage
{
  LogLevel Level;
  ::std::string Text;
  Label MessageLabel;
};

class CaptureSink final : public ILogSink
{
public:
  ::std::vector<CapturedMessage> Messages;

  void Write(const LogMessage& msg) noexcept override
  {
    Messages.push_back({.Level = msg.Level, .Text = msg.Text, .MessageLabel = msg.MessageLabel});
  }
};

}  // namespace

TEST_CASE("ImmediateLogger Init/Shutdown", "[runtime][logger]")
{
  ImmediateLogger logger;
  REQUIRE(logger.Init());
  logger.Shutdown();
}

TEST_CASE("ImmediateLogger delivers to sink", "[runtime][logger]")
{
  ImmediateLogger logger;
  logger.Init();

  CaptureSink sink;
  logger.AddSink(&sink);

  Label testLabel = MakeLabel("test.logger");
  logger.Log(LogLevel::Info, testLabel, "hello %s", "world");

  REQUIRE(sink.Messages.size() == 1);
  REQUIRE(sink.Messages[0].Level == LogLevel::Info);
  REQUIRE(sink.Messages[0].Text == "hello world");
  REQUIRE(sink.Messages[0].MessageLabel == testLabel);

  logger.RemoveSink(&sink);
  logger.Shutdown();
}

TEST_CASE("ImmediateLogger respects log level", "[runtime][logger]")
{
  ImmediateLogger logger;
  logger.Init();
  logger.SetLevel(LogLevel::Warn);

  CaptureSink sink;
  logger.AddSink(&sink);

  Label label = MakeLabel("test");
  logger.Log(LogLevel::Debug, label, "should be filtered");
  logger.Log(LogLevel::Info, label, "should be filtered");
  logger.Log(LogLevel::Warn, label, "should appear");
  logger.Log(LogLevel::Error, label, "should appear");

  REQUIRE(sink.Messages.size() == 2);
  REQUIRE(sink.Messages[0].Level == LogLevel::Warn);
  REQUIRE(sink.Messages[1].Level == LogLevel::Error);

  logger.RemoveSink(&sink);
  logger.Shutdown();
}

TEST_CASE("ImmediateLogger multiple sinks", "[runtime][logger]")
{
  ImmediateLogger logger;
  logger.Init();

  CaptureSink sink1;
  CaptureSink sink2;
  logger.AddSink(&sink1);
  logger.AddSink(&sink2);

  Label label = MakeLabel("multi");
  logger.Log(LogLevel::Info, label, "broadcast");

  REQUIRE(sink1.Messages.size() == 1);
  REQUIRE(sink2.Messages.size() == 1);

  logger.RemoveSink(&sink1);
  logger.RemoveSink(&sink2);
  logger.Shutdown();
}

TEST_CASE("ImmediateLogger level getter/setter", "[runtime][logger]")
{
  ImmediateLogger logger;
  logger.Init();

  REQUIRE(logger.Level() == LogLevel::Info);
  logger.SetLevel(LogLevel::Error);
  REQUIRE(logger.Level() == LogLevel::Error);

  logger.Shutdown();
}

TEST_CASE("ImmediateLogger Flush doesn't crash", "[runtime][logger]")
{
  ImmediateLogger logger;
  logger.Init();
  logger.Flush();
  logger.Shutdown();
}

TEST_CASE("ImmediateLogger thread-safe mode", "[runtime][logger]")
{
  ImmediateLogger logger;
  logger.Init();
  logger.SetThreadSafe(true);

  CaptureSink sink;
  logger.AddSink(&sink);

  Label label = MakeLabel("threadsafe");
  logger.Log(LogLevel::Info, label, "concurrent safe");

  REQUIRE(sink.Messages.size() == 1);

  logger.RemoveSink(&sink);
  logger.Shutdown();
}
