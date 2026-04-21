#include "gecko/graphics/graphics_device.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::graphics;
using namespace gecko::platform;

TEST_CASE("GraphicsDevice constructs without crashing", "[graphics][device]")
{
  GraphicsDevice device;
  REQUIRE(true);
}

TEST_CASE("GraphicsDevice CreateSwapchain returns invalid swapchain",
          "[graphics][device]")
{
  GraphicsDevice device;

  NativeWindowHandle native{};
  WindowDesc         wDesc{};
  SwapchainDesc      sDesc{.Width = 800, .Height = 600};

  Swapchain sc = device.CreateSwapchain(native, wDesc, sDesc);
  REQUIRE_FALSE(sc.IsValid());
}

TEST_CASE("GraphicsDevice CreateGraphicsCommandList returns valid command list",
          "[graphics][device]")
{
  GraphicsDevice device;

  auto cmdList = device.CreateGraphicsCommandList();
  REQUIRE(cmdList != nullptr);
  REQUIRE(cmdList->IsValid());
}

TEST_CASE("GraphicsDevice CreateComputeCommandList returns valid command list",
          "[graphics][device]")
{
  GraphicsDevice device;

  auto cmdList = device.CreateComputeCommandList();
  REQUIRE(cmdList != nullptr);
  REQUIRE(cmdList->IsValid());
}

TEST_CASE("GraphicsDevice resource creation returns invalid objects",
          "[graphics][device]")
{
  GraphicsDevice device;

  SECTION("CreateRenderTarget")
  {
    RenderTargetDesc desc;
    desc.Width                  = 800;
    desc.Height                 = 600;
    desc.NumRenderTargets       = 1;
    desc.RenderTargetFormats[0] = DataFormat::R8G8B8A8_UNORM;
    RenderTarget rt             = device.CreateRenderTarget(desc);
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

TEST_CASE("GraphicsDevice GetCurrentBackBufferIndex returns 0",
          "[graphics][device]")
{
  GraphicsDevice device;
  Swapchain      sc{};
  REQUIRE(device.GetCurrentBackBufferIndex(sc) == 0);
}

TEST_CASE("Multiple GraphicsDevice instances are independent",
          "[graphics][device]")
{
  GraphicsDevice deviceA;
  GraphicsDevice deviceB;

  auto cmdA = deviceA.CreateGraphicsCommandList();
  auto cmdB = deviceB.CreateGraphicsCommandList();
  REQUIRE(cmdA != nullptr);
  REQUIRE(cmdB != nullptr);
}
