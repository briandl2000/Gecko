#include "feature_platform_scope.h"
#include "gecko/platform/monitor.h"
#include "gecko/platform/platform_events.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::platform;

TEST_CASE("Live backend: enumerate monitors returns at least one",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  const u32 count = scope.Ctx.Monitors().GetMonitorCount();
  REQUIRE(count >= 1);
}

TEST_CASE("Live backend: primary monitor exists",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  MonitorHandle primary = scope.Ctx.Monitors().GetPrimaryMonitor();
  REQUIRE(primary.IsValid());
}

TEST_CASE("Live backend: monitor handle by index is valid",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  const u32 count = scope.Ctx.Monitors().GetMonitorCount();
  REQUIRE(count >= 1);

  MonitorHandle h = scope.Ctx.Monitors().GetMonitorHandle(0);
  REQUIRE(h.IsValid());

  // Out-of-range index fails cleanly
  MonitorHandle invalid = scope.Ctx.Monitors().GetMonitorHandle(count);
  REQUIRE_FALSE(invalid.IsValid());
}

TEST_CASE("Live backend: monitor properties are populated",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  MonitorHandle h = scope.Ctx.Monitors().GetMonitorHandle(0);

  MonitorInfo info = scope.Ctx.Monitors().GetMonitorProperties(h);
  REQUIRE(info.Bounds.Width() > 0);
  REQUIRE(info.Bounds.Height() > 0);
  REQUIRE(info.Dpi > 0);
  REQUIRE(info.DpiScale > 0.0F);
  REQUIRE(info.RefreshRateMilliHz > 0);
}

TEST_CASE("Live backend: monitor name is non-empty",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  MonitorHandle h = scope.Ctx.Monitors().GetMonitorHandle(0);

  MonitorInfo info = scope.Ctx.Monitors().GetMonitorProperties(h);
  REQUIRE(info.Name[0] != '\0');
}

TEST_CASE("Live backend: monitor bounds and work area are valid",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  MonitorHandle h = scope.Ctx.Monitors().GetMonitorHandle(0);

  MonitorBounds mb = scope.Ctx.Monitors().GetMonitorBounds(h);
  REQUIRE_FALSE(mb.Bounds.IsEmpty());
  REQUIRE_FALSE(mb.WorkArea.IsEmpty());
}

TEST_CASE("Live backend: all monitors have consistent data",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  const u32 count = scope.Ctx.Monitors().GetMonitorCount();
  bool foundPrimary = false;

  for (u32 i = 0; i < count; ++i)
  {
    MonitorHandle h = scope.Ctx.Monitors().GetMonitorHandle(i);
    REQUIRE(h.IsValid());

    MonitorInfo info = scope.Ctx.Monitors().GetMonitorProperties(h);
    REQUIRE(info.Bounds.Width() > 0);
    REQUIRE(info.Bounds.Height() > 0);
    REQUIRE(info.Dpi > 0);

    if (info.IsPrimary)
      foundPrimary = true;
  }

  REQUIRE(foundPrimary);
}

TEST_CASE("Live backend: pump monitor events does not crash",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  scope.Ctx.Monitors().EnumerateMonitors();

  // Pump several times — should be safe even with no changes
  for (int i = 0; i < 5; ++i)
    scope.Ctx.PumpEvents();
}
