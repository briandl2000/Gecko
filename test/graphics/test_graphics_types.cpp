#include "gecko/graphics/graphics_types.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::graphics;

// ── FormatSizeInBytes ──────────────────────────────────────────────────────

TEST_CASE("FormatSizeInBytes returns correct sizes", "[graphics][objects]")
{
  SECTION("4-byte formats")
  {
    REQUIRE(FormatSizeInBytes(DataFormat::R8G8B8A8_SRGB) == 4);
    REQUIRE(FormatSizeInBytes(DataFormat::R8G8B8A8_UNORM) == 4);
    REQUIRE(FormatSizeInBytes(DataFormat::R32_FLOAT) == 4);
    REQUIRE(FormatSizeInBytes(DataFormat::R32_UINT) == 4);
    REQUIRE(FormatSizeInBytes(DataFormat::R32_INT) == 4);
  }

  SECTION("8-byte formats")
  {
    REQUIRE(FormatSizeInBytes(DataFormat::R32G32_FLOAT) == 8);
    REQUIRE(FormatSizeInBytes(DataFormat::R16G16B16A16_FLOAT) == 8);
  }

  SECTION("12-byte formats")
  {
    REQUIRE(FormatSizeInBytes(DataFormat::R32G32B32_FLOAT) == 12);
  }

  SECTION("16-byte formats")
  {
    REQUIRE(FormatSizeInBytes(DataFormat::R32G32B32A32_FLOAT) == 16);
  }

  SECTION("None returns 0")
  {
    REQUIRE(FormatSizeInBytes(DataFormat::None) == 0);
  }
}

// ── CalculateNumberOfMips ─────────────────────────────────────────────────

TEST_CASE("CalculateNumberOfMips", "[graphics][objects]")
{
  SECTION("1x1 has 1 mip")
  {
    REQUIRE(CalculateNumberOfMips(1, 1) == 1);
  }

  SECTION("2x2 has 2 mips")
  {
    REQUIRE(CalculateNumberOfMips(2, 2) == 2);
  }

  SECTION("4x4 has 3 mips")
  {
    REQUIRE(CalculateNumberOfMips(4, 4) == 3);
  }

  SECTION("256x256 has 9 mips")
  {
    REQUIRE(CalculateNumberOfMips(256, 256) == 9);
  }

  SECTION("non-power-of-two 3x5")
  {
    // 3x5 -> 1x2 -> 1x1 = 3 steps after initial, so 4 mips... wait:
    // step 0: 3x5 (mips=1)
    // step 1: 1x2 (mips=2)
    // step 2: 1x1 (mips=3)
    REQUIRE(CalculateNumberOfMips(3, 5) == 3);
  }

  SECTION("zero width returns 0")
  {
    REQUIRE(CalculateNumberOfMips(0, 4) == 0);
  }

  SECTION("zero height returns 0")
  {
    REQUIRE(CalculateNumberOfMips(4, 0) == 0);
  }

  SECTION("zero width and height returns 0")
  {
    REQUIRE(CalculateNumberOfMips(0, 0) == 0);
  }
}

// ── VertexAttribute ───────────────────────────────────────────────────────

TEST_CASE("VertexAttribute default is invalid", "[graphics][objects]")
{
  VertexAttribute attr;
  REQUIRE_FALSE(attr.IsValid());
  REQUIRE_FALSE(static_cast<bool>(attr));
}

TEST_CASE("VertexAttribute constructed from format is valid", "[graphics][objects]")
{
  VertexAttribute attr {DataFormat::R32G32B32_FLOAT, "Position"};
  REQUIRE(attr.IsValid());
  REQUIRE(attr.Size == 12);
  REQUIRE(attr.Offset == 0);
}

// ── VertexLayout ──────────────────────────────────────────────────────────

TEST_CASE("VertexLayout default is invalid", "[graphics][objects]")
{
  VertexLayout layout;
  REQUIRE_FALSE(layout.IsValid());
}

TEST_CASE("VertexLayout with attributes is valid", "[graphics][objects]")
{
  VertexLayout layout;
  layout.AddAttribute(DataFormat::R32G32B32_FLOAT, "Position");
  layout.AddAttribute(DataFormat::R32G32_FLOAT, "UV");

  REQUIRE(layout.IsValid());
  REQUIRE(layout.NumAttributes == 2);
  REQUIRE(layout.StrideInBytes == 20);
  REQUIRE(layout.Attributes[0].Offset == 0);
  REQUIRE(layout.Attributes[1].Offset == 12);
}

TEST_CASE("VertexLayout AddAttribute ignores DataFormat::None", "[graphics][objects]")
{
  VertexLayout layout;
  layout.AddAttribute(DataFormat::None, "Bad");
  REQUIRE(layout.NumAttributes == 0);
  REQUIRE(layout.StrideInBytes == 0);
  REQUIRE_FALSE(layout.IsValid());
}

TEST_CASE("VertexLayout AddAttribute mixed valid and invalid formats", "[graphics][objects]")
{
  VertexLayout layout;
  layout.AddAttribute(DataFormat::R32G32B32_FLOAT, "Position");
  layout.AddAttribute(DataFormat::None, "Bad");
  layout.AddAttribute(DataFormat::R32G32_FLOAT, "UV");

  REQUIRE(layout.NumAttributes == 2);
  REQUIRE(layout.StrideInBytes == 20);
  REQUIRE(layout.IsValid());
}

// ── VertexBufferDesc ──────────────────────────────────────────────────────

TEST_CASE("VertexBufferDesc default is invalid", "[graphics][objects]")
{
  VertexBufferDesc desc;
  REQUIRE_FALSE(desc.IsValid());
}

TEST_CASE("VertexBufferDesc valid when all fields set", "[graphics][objects]")
{
  VertexBufferDesc desc {
      .NumVertices = 100,
      .VertexSize = 12,
      .Memory = MemoryType::Dedicated,
  };
  REQUIRE(desc.IsValid());
}

TEST_CASE("VertexBufferDesc invalid when NumVertices is 0", "[graphics][objects]")
{
  VertexBufferDesc desc {
      .NumVertices = 0,
      .VertexSize = 12,
      .Memory = MemoryType::Dedicated,
  };
  REQUIRE_FALSE(desc.IsValid());
}

// ── TextureDesc ───────────────────────────────────────────────────────────

TEST_CASE("TextureDesc default is invalid", "[graphics][objects]")
{
  TextureDesc desc;
  REQUIRE_FALSE(desc.IsValid());
}

TEST_CASE("TextureDesc valid with all required fields", "[graphics][objects]")
{
  TextureDesc desc {
      .Width = 512,
      .Height = 512,
      .Format = DataFormat::R8G8B8A8_UNORM,
      .Type = TextureType::Tex2D,
      .Memory = MemoryType::Dedicated,
  };
  REQUIRE(desc.IsValid());
}

// ── RenderTargetDesc ──────────────────────────────────────────────────────

TEST_CASE("RenderTargetDesc default is invalid", "[graphics][objects]")
{
  RenderTargetDesc desc;
  REQUIRE_FALSE(desc.IsValid());
}

TEST_CASE("RenderTargetDesc valid with color target", "[graphics][objects]")
{
  RenderTargetDesc desc;
  desc.Width = 1280;
  desc.Height = 720;
  desc.Format = DataFormat::R8G8B8A8_UNORM;
  REQUIRE(desc.IsValid());
}

TEST_CASE("RenderTargetDesc valid with depth target", "[graphics][objects]")
{
  RenderTargetDesc desc;
  desc.Width = 1280;
  desc.Height = 720;
  desc.Format = DataFormat::D32_FLOAT;
  REQUIRE(desc.IsValid());
  REQUIRE(desc.IsDepth());
}

TEST_CASE("RenderTargetDesc invalid when no format", "[graphics][objects]")
{
  RenderTargetDesc desc;
  desc.Width = 1280;
  desc.Height = 720;
  REQUIRE_FALSE(desc.IsValid());
}

// ── GraphicsPipelineDesc ──────────────────────────────────────────────────

TEST_CASE("GraphicsPipelineDesc default is invalid", "[graphics][objects]")
{
  GraphicsPipelineDesc desc;
  REQUIRE_FALSE(desc.IsValid());
}

TEST_CASE("GraphicsPipelineDesc valid with shader path and render target", "[graphics][objects]")
{
  static constexpr ::gecko::byte DummySpv[] = {::gecko::byte {0}};
  GraphicsPipelineDesc desc;
  desc.VertexShader = ShaderCode {
      .Format = ShaderFormat::SPIRV,
      .Bytes = {DummySpv, sizeof(DummySpv)},
  };
  desc.NumRenderTargets = 1;
  desc.RenderTargetFormats[0] = DataFormat::R8G8B8A8_UNORM;
  REQUIRE(desc.IsValid());
}

// ── ComputePipelineDesc ───────────────────────────────────────────────────

TEST_CASE("ComputePipelineDesc default is invalid", "[graphics][objects]")
{
  ComputePipelineDesc desc;
  REQUIRE_FALSE(desc.IsValid());
}

TEST_CASE("ComputePipelineDesc valid with shader path", "[graphics][objects]")
{
  static constexpr ::gecko::byte DummySpv[] = {::gecko::byte {0}};
  ComputePipelineDesc desc {.ComputeShader = ShaderCode {
                                .Format = ShaderFormat::SPIRV,
                                .Bytes = {DummySpv, sizeof(DummySpv)},
                            }};
  REQUIRE(desc.IsValid());
}

// ── SwapchainDesc ─────────────────────────────────────────────────────────

TEST_CASE("SwapchainDesc default is invalid", "[graphics][objects]")
{
  SwapchainDesc desc;
  REQUIRE_FALSE(desc.IsValid());
}

TEST_CASE("SwapchainDesc valid with width/height/format", "[graphics][objects]")
{
  SwapchainDesc desc {
      .Width = 1280,
      .Height = 720,
  };
  REQUIRE(desc.IsValid());
}

// ── ClearValue helpers ────────────────────────────────────────────────────

TEST_CASE("ClearValue::RenderTarget sets correct type and color", "[graphics][objects]")
{
  auto cv = ClearValue::RenderTarget(0.1F, 0.2F, 0.3F, 1.0F);
  REQUIRE(cv.Type == ClearValueType::RenderTarget);
  REQUIRE(cv.Color[0] == 0.1F);
  REQUIRE(cv.Color[3] == 1.0F);
}

TEST_CASE("ClearValue::DepthStencil sets correct type and values", "[graphics][objects]")
{
  auto cv = ClearValue::DepthStencil(1.0F, 0);
  REQUIRE(cv.Type == ClearValueType::DepthStencil);
  REQUIRE(cv.Depth == 1.0F);
  REQUIRE(cv.Stencil == 0);
}
