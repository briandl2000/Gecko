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

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  const u32 count = ::gecko::platform::GetMonitors()->GetMonitorCount();
  REQUIRE(count >= 1);
}

TEST_CASE("Live backend: primary monitor exists",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  MonitorHandle primary = ::gecko::platform::GetMonitors()->GetPrimaryMonitor();
  REQUIRE(primary.IsValid());
}

TEST_CASE("Live backend: monitor handle by index is valid",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  const u32 count = ::gecko::platform::GetMonitors()->GetMonitorCount();
  REQUIRE(count >= 1);

  MonitorHandle h = ::gecko::platform::GetMonitors()->GetMonitorHandle(0);
  REQUIRE(h.IsValid());

  // Out-of-range index fails cleanly
  MonitorHandle invalid =
      ::gecko::platform::GetMonitors()->GetMonitorHandle(count);
  REQUIRE_FALSE(invalid.IsValid());
}

TEST_CASE("Live backend: monitor properties are populated",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  MonitorHandle h = ::gecko::platform::GetMonitors()->GetMonitorHandle(0);

  MonitorInfo info = ::gecko::platform::GetMonitors()->GetMonitorProperties(h);
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

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  MonitorHandle h = ::gecko::platform::GetMonitors()->GetMonitorHandle(0);

  MonitorInfo info = ::gecko::platform::GetMonitors()->GetMonitorProperties(h);
  REQUIRE(info.Name[0] != '\0');
}

TEST_CASE("Live backend: monitor bounds and work area are valid",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  MonitorHandle h = ::gecko::platform::GetMonitors()->GetMonitorHandle(0);

  MonitorBounds mb = ::gecko::platform::GetMonitors()->GetMonitorBounds(h);
  REQUIRE_FALSE(mb.Bounds.IsEmpty());
  REQUIRE_FALSE(mb.WorkArea.IsEmpty());
}

TEST_CASE("Live backend: all monitors have consistent data",
          "[feature][platform][monitor]")
{
  test::FeaturePlatformScope scope;

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  const u32 count = ::gecko::platform::GetMonitors()->GetMonitorCount();
  bool foundPrimary = false;

  for (u32 i = 0; i < count; ++i)
  {
    MonitorHandle h = ::gecko::platform::GetMonitors()->GetMonitorHandle(i);
    REQUIRE(h.IsValid());

    MonitorInfo info =
        ::gecko::platform::GetMonitors()->GetMonitorProperties(h);
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

  ::gecko::platform::GetMonitors()->EnumerateMonitors();

  // Pump several times — should be safe even with no changes
  for (int i = 0; i < 5; ++i)
    ::gecko::platform::PumpEvents();
}
