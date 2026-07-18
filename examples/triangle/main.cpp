#include "gecko/gecko.h"
#include "gecko/platform/platform_events.h"

namespace {

constexpr gecko::Label ExampleLabel = gecko::MakeLabel("example.triangle");

struct Vertex
{
  float Position[3];
  float Color[3];
};

constexpr Vertex Vertices[] = {
    {{0.0F, -0.6F, 0.0F}, {1.0F, 0.2F, 0.1F}},
    {{0.6F, 0.6F, 0.0F}, {0.1F, 1.0F, 0.2F}},
    {{-0.6F, 0.6F, 0.0F}, {0.2F, 0.3F, 1.0F}},
};

bool Running = true;

void OnClose(void*, const gecko::EventMeta&, gecko::EventView) noexcept
{
  Running = false;
}

}  // namespace

int main()
{
  gecko::GeckoConfig config {};
  config.AppName = "Gecko Triangle Example";
  config.EnableGraphicsDebug = true;
  if (gecko::Initialize(config) != gecko::InitializeResult::Success)
    return 1;

  auto* windows = gecko::platform::GetWindows();
  auto* device = gecko::graphics::GetGraphicsDevice();
  if (windows == nullptr || device == nullptr)
  {
    gecko::Shutdown();
    return 2;
  }

  gecko::platform::WindowDesc windowDesc {};
  windowDesc.Title = "Gecko Vulkan Triangle";
  windowDesc.Size = {960, 540};
  windowDesc.Resizable = false;
  const gecko::platform::WindowHandle window = windows->CreateWindow(windowDesc);
  if (!window.IsValid())
  {
    gecko::Shutdown();
    return 3;
  }

  const gecko::platform::Extent2D size = windows->GetClientSize(window);
  gecko::graphics::SwapchainDesc swapchainDesc {};
  swapchainDesc.Width = size.Width;
  swapchainDesc.Height = size.Height;
  swapchainDesc.Format = gecko::graphics::DataFormat::R8G8B8A8_UNORM;
  swapchainDesc.DebugName = "Triangle Swapchain";
  auto swapchain = device->CreateSwapchain(windows->GetNativeWindowHandle(window), swapchainDesc);

  gecko::graphics::VertexBufferDesc vertexDesc {};
  vertexDesc.NumVertices = 3;
  vertexDesc.VertexSize = sizeof(Vertex);
  vertexDesc.Memory = gecko::graphics::MemoryType::Dedicated;
  vertexDesc.DebugName = "Triangle Vertices";
  auto vertices = device->CreateVertexBuffer(vertexDesc);
  device->UploadBufferData(vertices, {reinterpret_cast<const gecko::byte*>(Vertices), sizeof(Vertices)});

  gecko::graphics::VertexLayout layout {};
  layout.AddAttribute(gecko::graphics::DataFormat::R32G32B32_FLOAT, "Position");
  layout.AddAttribute(gecko::graphics::DataFormat::R32G32B32_FLOAT, "Color");

  gecko::graphics::GraphicsPipelineDesc pipelineDesc {};
  pipelineDesc.VertexShader = {
      .Format = gecko::graphics::ShaderFormat::SPIRV,
      .Path = "shaders/triangle.vert.spv",
  };
  pipelineDesc.PixelShader = {
      .Format = gecko::graphics::ShaderFormat::SPIRV,
      .Path = "shaders/triangle.frag.spv",
  };
  pipelineDesc.Layout = layout;
  pipelineDesc.NumRenderTargets = 1;
  pipelineDesc.RenderTargetFormats[0] = swapchainDesc.Format;
  pipelineDesc.DebugName = "Triangle Pipeline";
  auto pipeline = device->CreateGraphicsPipeline(pipelineDesc);
  if (!swapchain.IsValid() || !vertices.IsValid() || !pipeline.IsValid())
  {
    GECKO_ERROR(ExampleLabel, "failed to create triangle rendering resources");
    pipeline = {};
    vertices = {};
    if (swapchain.IsValid())
      device->DestroySwapchain(swapchain);
    windows->DestroyWindow(window);
    gecko::Shutdown();
    return 4;
  }

  gecko::EventSubscription close =
      gecko::SubscribeEvent(gecko::platform::events::WindowCloseRequested, OnClose, nullptr);
  GECKO_INFO(ExampleLabel, "drawing with precompiled shaders from the shaders directory");

  while (Running && windows->IsWindowAlive(window))
  {
    gecko::platform::PumpEvents();
    (void)gecko::DispatchEvents();

    const gecko::graphics::FrameContext frame = device->BeginFrame(swapchain);
    if (!frame.Valid)
      continue;

    auto commands = device->CreateGraphicsCommandList();
    commands->Begin();
    const gecko::graphics::ClearValue clear = gecko::graphics::ClearValue::RenderTarget(0.015F, 0.02F, 0.035F, 1.0F);
    const gecko::graphics::BeginRenderingInfo rendering {
        .Colors = {&frame.BackBuffer, 1},
        .ClearColors = {&clear, 1},
    };
    commands->BeginRendering(rendering);
    commands->SetViewport(0.0F, 0.0F, static_cast<float>(size.Width), static_cast<float>(size.Height));
    commands->SetScissor(0, 0, size.Width, size.Height);
    commands->BindPipeline(pipeline);
    commands->BindVertexBuffer(vertices);
    commands->Draw(3);
    commands->EndRendering();
    commands->End();
    device->ExecuteGraphicsCommandList(gecko::Move(commands));
    device->Present(frame);
  }

  close.Reset();
  pipeline = {};
  vertices = {};
  device->DestroySwapchain(swapchain);
  windows->DestroyWindow(window);
  gecko::Shutdown();
  return 0;
}
