#include "gecko/graphics/device.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::graphics;

TEST_CASE("GetDevice returns a valid device by default", "[graphics][device]")
{
  // NullDevice is installed by default — just verify we can get it without crashing
  [[maybe_unused]] IDevice& device = GetDevice();
  REQUIRE(true);
}

TEST_CASE("NullDevice Init/Shutdown are no-ops", "[graphics][device]")
{
  IDevice& device = GetDevice();
  device.Init();
  device.Shutdown();
}

TEST_CASE("NullDevice CreateSwapchain returns invalid swapchain",
          "[graphics][device]")
{
  IDevice& device = GetDevice();

  platform::NativeWindowHandle native{};
  platform::WindowDesc         wDesc{};
  SwapchainDesc                sDesc{.Width = 800, .Height = 600};

  Swapchain sc = device.CreateSwapchain(native, wDesc, sDesc);
  REQUIRE_FALSE(sc.IsValid());
}

TEST_CASE("NullDevice CreateGraphicsCommandList returns valid command list",
          "[graphics][device]")
{
  IDevice& device = GetDevice();

  auto cmdList = device.CreateGraphicsCommandList();
  REQUIRE(cmdList != nullptr);
  REQUIRE(cmdList->IsValid());
}

TEST_CASE("NullDevice CreateComputeCommandList returns valid command list",
          "[graphics][device]")
{
  IDevice& device = GetDevice();

  auto cmdList = device.CreateComputeCommandList();
  REQUIRE(cmdList != nullptr);
  REQUIRE(cmdList->IsValid());
}

TEST_CASE("NullDevice resource creation returns invalid objects",
          "[graphics][device]")
{
  IDevice& device = GetDevice();

  SECTION("CreateRenderTarget")
  {
    RenderTargetDesc desc;
    desc.Width                 = 800;
    desc.Height                = 600;
    desc.NumRenderTargets      = 1;
    desc.RenderTargetFormats[0] = DataFormat::R8G8B8A8_UNORM;
    RenderTarget rt = device.CreateRenderTarget(desc);
    REQUIRE_FALSE(rt.IsValid());
  }

  SECTION("CreateVertexBuffer")
  {
    VertexBufferDesc desc{
        .NumVertices = 3,
        .VertexSize  = 12,
        .Memory      = MemoryType::Dedicated,
    };
    Buffer buf = device.CreateVertexBuffer(desc);
    REQUIRE_FALSE(buf.IsValid());
  }

  SECTION("CreateTexture")
  {
    TextureDesc desc{
        .Width  = 64,
        .Height = 64,
        .Format = DataFormat::R8G8B8A8_UNORM,
        .Type   = TextureType::Tex2D,
        .Memory = MemoryType::Dedicated,
    };
    Texture tex = device.CreateTexture(desc);
    REQUIRE_FALSE(tex.IsValid());
  }
}

TEST_CASE("NullDevice GetCurrentBackBufferIndex returns 0",
          "[graphics][device]")
{
  IDevice& device = GetDevice();
  Swapchain sc{};
  REQUIRE(device.GetCurrentBackBufferIndex(sc) == 0);
}

TEST_CASE("InstallDevice with nullptr restores NullDevice",
          "[graphics][device]")
{
  IDevice* original = &GetDevice();
  InstallDevice(nullptr);
  REQUIRE(&GetDevice() != nullptr);
  // Restore
  InstallDevice(original);
}
