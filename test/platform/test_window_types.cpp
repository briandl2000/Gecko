#include "gecko/platform/monitor.h"
#include "gecko/platform/window.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko::platform;

TEST_CASE("WindowHandle default is invalid", "[platform][window]")
{
  WindowHandle h;
  REQUIRE_FALSE(h.IsValid());
  REQUIRE(h.Id == 0);
}

TEST_CASE("WindowHandle explicit construction", "[platform][window]")
{
  WindowHandle h {42};
  REQUIRE(h.IsValid());
  REQUIRE(h.Id == 42);
}

TEST_CASE("WindowHandle equality", "[platform][window]")
{
  WindowHandle a {1};
  WindowHandle b {1};
  WindowHandle c {2};

  REQUIRE(a == b);
  REQUIRE(a != c);
}

TEST_CASE("WindowHandle reset", "[platform][window]")
{
  WindowHandle h {5};
  REQUIRE(h.IsValid());
  h.Reset();
  REQUIRE_FALSE(h.IsValid());
}

TEST_CASE("MonitorHandle default is invalid", "[platform][window]")
{
  MonitorHandle h;
  REQUIRE_FALSE(h.IsValid());
}

TEST_CASE("MonitorHandle equality", "[platform][window]")
{
  MonitorHandle a {10};
  MonitorHandle b {10};
  MonitorHandle c {20};

  REQUIRE(a == b);
  REQUIRE(a != c);
}

TEST_CASE("Extent2D default is zero", "[platform][window]")
{
  Extent2D e;
  REQUIRE(e.Width == 0);
  REQUIRE(e.Height == 0);
}

TEST_CASE("WindowDesc defaults", "[platform][window]")
{
  WindowDesc desc;
  REQUIRE(desc.Size.X == 1280);
  REQUIRE(desc.Size.Y == 720);
  REQUIRE(desc.Resizable == true);
  REQUIRE(desc.Visible == true);
  REQUIRE(desc.HighDpi == true);
  REQUIRE(desc.Mode == WindowMode::Windowed);
}

TEST_CASE("NativeWindowHandle defaults", "[platform][window]")
{
  NativeWindowHandle nh;
  REQUIRE(nh.Backend == DisplayBackendKind::Unknown);
  REQUIRE(nh.Handle == nullptr);
  REQUIRE(nh.Display == nullptr);
}

TEST_CASE("WindowDesc Decorated defaults to true", "[platform][window]")
{
  WindowDesc desc;
  REQUIRE(desc.Decorated == true);
}

TEST_CASE("WindowState enum values", "[platform][window]")
{
  REQUIRE(WindowState::Normal != WindowState::Minimized);
  REQUIRE(WindowState::Normal != WindowState::Maximized);
  REQUIRE(WindowState::Normal != WindowState::Hidden);
  REQUIRE(WindowState::Minimized != WindowState::Maximized);
}
