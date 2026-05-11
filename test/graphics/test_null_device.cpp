#include "gecko/graphics/graphics_device.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::graphics;
using namespace gecko::platform;

TEST_CASE("CreateGraphicsDevice returns a device", "[graphics][device]")
{
  auto device = CreateGraphicsDevice();
  REQUIRE(device != nullptr);
}

TEST_CASE("CreateSwapchain returns invalid swapchain for NullDevice", "[graphics][device]")
{
  auto device = CreateGraphicsDevice();

  NativeWindowHandle native {};
  SwapchainDesc sDesc {.Width = 800, .Height = 600};

  Swapchain sc = device->CreateSwapchain(native, sDesc);
  REQUIRE_FALSE(sc.IsValid());
}

TEST_CASE("CreateGraphicsCommandList returns valid command list", "[graphics][device]")
{
  auto device = CreateGraphicsDevice();
  auto cmdList = device->CreateGraphicsCommandList();
  REQUIRE(cmdList != nullptr);
  REQUIRE(cmdList->IsValid());
}

TEST_CASE("CreateComputeCommandList returns valid command list", "[graphics][device]")
{
  auto device = CreateGraphicsDevice();
  auto cmdList = device->CreateComputeCommandList();
  REQUIRE(cmdList != nullptr);
  REQUIRE(cmdList->IsValid());
}

TEST_CASE("NullDevice resource creation returns invalid objects", "[graphics][device]")
{
  auto device = CreateGraphicsDevice();

  SECTION("CreateRenderTarget")
  {
    RenderTargetDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = DataFormat::R8G8B8A8_UNORM;
    RenderTarget rt = device->CreateRenderTarget(desc);
    REQUIRE_FALSE(rt.IsValid());
  }

  SECTION("CreateVertexBuffer")
  {
    VertexBufferDesc desc {
        .NumVertices = 3,
        .VertexSize = 12,
        .Memory = MemoryType::Dedicated,
    };
    Buffer buf = device->CreateVertexBuffer(desc);
    REQUIRE_FALSE(buf.IsValid());
  }

  SECTION("CreateTexture")
  {
    TextureDesc desc {
        .Width = 64,
        .Height = 64,
        .Format = DataFormat::R8G8B8A8_UNORM,
        .Type = TextureType::Tex2D,
        .Memory = MemoryType::Dedicated,
    };
    Texture tex = device->CreateTexture(desc);
    REQUIRE_FALSE(tex.IsValid());
  }
}

TEST_CASE("BeginFrame returns invalid FrameContext for NullDevice", "[graphics][device]")
{
  auto device = CreateGraphicsDevice();
  Swapchain sc {};
  FrameContext f = device->BeginFrame(sc);
  REQUIRE_FALSE(f.Valid);
}

TEST_CASE("Multiple independent devices can be created", "[graphics][device]")
{
  auto deviceA = CreateGraphicsDevice();
  auto deviceB = CreateGraphicsDevice();

  auto cmdA = deviceA->CreateGraphicsCommandList();
  auto cmdB = deviceB->CreateGraphicsCommandList();
  REQUIRE(cmdA != nullptr);
  REQUIRE(cmdB != nullptr);
}
