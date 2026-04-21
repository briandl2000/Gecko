#include <gecko/core/boot.h>
#include <gecko/core/scope.h>
#include <gecko/core/services.h>
#include <gecko/core/services/events.h>
#include <gecko/core/services/log.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/version.h>
#include <gecko/graphics/graphics_device.h>
#include <gecko/platform/platform_context.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/module_registry.h>
#include <gecko/runtime/ring_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>

using namespace gecko;
using namespace gecko::platform;
using namespace gecko::graphics;

namespace app::graphics_example::labels {
inline constexpr ::gecko::Label App =
    ::gecko::MakeLabel("app.graphics_example");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.graphics_example.main");
}  // namespace app::graphics_example::labels

namespace {

class AppModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] ::gecko::Label RootLabel() const noexcept override
  {
    return app::graphics_example::labels::App;
  }

  [[nodiscard]] bool Startup(
      ::gecko::IModuleRegistry& /*modules*/) noexcept override
  {
    return true;
  }

  void Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept override {}
};

AppModule g_AppModule;

struct Vertex
{
  float Position[3];
  float Color[3];
};

constexpr Vertex k_TriangleVertices[] = {
    {{ 0.0F,  0.5F, 0.0F}, {1.0F, 0.0F, 0.0F}},  // top    — red
    {{ 0.5F, -0.5F, 0.0F}, {0.0F, 1.0F, 0.0F}},  // right  — green
    {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},  // left   — blue
};

}  // namespace

int main()
{
  runtime::TrackingAllocator trackingAlloc;
  runtime::RingProfiler      ringProfiler(1 << 16);
  runtime::RingLogger        ringLogger(1 << 16);
  runtime::ModuleRegistry    moduleRegistry;
  runtime::EventBus          eventBus;
  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  GECKO_BOOT((Services{.Allocator = &trackingAlloc,
                       .JobSystem  = &jobSystem,
                       .Profiler   = &ringProfiler,
                       .Logger     = &ringLogger,
                       .Modules    = &moduleRegistry,
                       .EventBus   = &eventBus}));

  runtime::ConsoleLogSink consoleSink;
  if (auto* logger = GetLogger())
  {
    consoleSink.RegisterWith(logger);
    logger->SetLevel(LogLevel::Info);
  }

  {
    GECKO_FUNC(app::graphics_example::labels::Main);
    GECKO_INFO(app::graphics_example::labels::Main, gecko::VersionFullString());

    (void)InstallModule(runtime::GetModule());
    (void)InstallModule(platform::GetModule());
    (void)InstallModule(g_AppModule);

    // ── Window ────────────────────────────────────────────────────
    PlatformContext ctx(PlatformConfig{});

    WindowDesc windowDesc;
    windowDesc.Title     = "Gecko Graphics Example";
    windowDesc.Size      = {1280, 720};
    windowDesc.Visible   = true;
    windowDesc.Resizable = true;
    windowDesc.Mode      = WindowMode::Windowed;

    WindowHandle window = ctx.Windows().CreateWindow(windowDesc);
    if (!window.IsValid())
    {
      GECKO_ERROR(app::graphics_example::labels::Main,
                  "Failed to create window");
      return 1;
    }
    GECKO_INFO(app::graphics_example::labels::Main, "Window created");

    // ── Graphics device ──────────────────────────────────────────
    auto device = CreateGraphicsDevice(
        GraphicsDeviceDesc{.Backend = GraphicsBackend::Vulkan,
                           .Debug   = true,
                           .AppName = "graphics_example"});
    GECKO_INFO(app::graphics_example::labels::Main, "Graphics device created");

    // ── Swapchain ────────────────────────────────────────────────
    NativeWindowHandle native   = ctx.Windows().GetNativeWindowHandle(window);
    Extent2D           clientSz = ctx.Windows().GetClientSize(window);

    SwapchainDesc scDesc;
    scDesc.Width          = clientSz.Width;
    scDesc.Height         = clientSz.Height;
    scDesc.NumBackBuffers = 2;
    scDesc.Format         = DataFormat::R8G8B8A8_UNORM;
    scDesc.VSync          = true;

    Swapchain swapchain = device->CreateSwapchain(native, scDesc);
    if (!swapchain.IsValid())
    {
      GECKO_WARN(app::graphics_example::labels::Main,
                 "No concrete graphics backend; running without a real swapchain"
                 " (NullDevice presents are no-ops)");
    }
    else
    {
      GECKO_INFO(app::graphics_example::labels::Main,
                 "Swapchain created (%ux%u, %u back buffers)",
                 scDesc.Width, scDesc.Height, scDesc.NumBackBuffers);
    }

    // ── Vertex buffer ─────────────────────────────────────────────
    VertexBufferDesc vbDesc;
    vbDesc.NumVertices = 3;
    vbDesc.VertexSize  = sizeof(Vertex);
    vbDesc.Memory      = MemoryType::Dedicated;

    Buffer vertexBuffer = device->CreateVertexBuffer(vbDesc);
    if (vertexBuffer.IsValid())
    {
      const auto* raw  = reinterpret_cast<const gecko::byte*>(k_TriangleVertices);
      const usize size = sizeof(k_TriangleVertices);
      device->UploadBufferData(vertexBuffer, {raw, size});
      GECKO_INFO(app::graphics_example::labels::Main, "Vertex buffer uploaded");
    }

    // ── Graphics pipeline ─────────────────────────────────────────
    VertexLayout layout;
    layout.AddAttribute(DataFormat::R32G32B32_FLOAT, "a_Position");
    layout.AddAttribute(DataFormat::R32G32B32_FLOAT, "a_Color");

    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.VertexShaderPath       = "shaders/triangle.vert.spv";
    pipelineDesc.PixelShaderPath        = "shaders/triangle.frag.spv";
    pipelineDesc.Layout                 = layout;
    pipelineDesc.NumRenderTargets       = 1;
    pipelineDesc.RenderTargetFormats[0] = swapchain.IsValid()
                                              ? swapchain.Desc.Format
                                              : DataFormat::R8G8B8A8_UNORM;
    pipelineDesc.Culling                = CullMode::None;

    GraphicsPipeline pipeline = device->CreateGraphicsPipeline(pipelineDesc);
    if (!pipeline.IsValid())
    {
      GECKO_WARN(app::graphics_example::labels::Main,
                 "Pipeline creation failed — will skip draw calls");
    }
    else
    {
      GECKO_INFO(app::graphics_example::labels::Main, "Graphics pipeline created");
    }

    // ── Event subscriptions ───────────────────────────────────────
    bool running = true;

    auto closeSub = SubscribeEvent(
        events::WindowCloseRequested,
        [](void* user, const EventMeta&, EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowCloseRequestedPayload*>(
                  view.Data());
          (void)payload;
          *static_cast<bool*>(user) = false;
        },
        &running);

    struct ResizeState
    {
      GraphicsDevice* Device;
      Swapchain*      SC;
    };
    ResizeState resizeState{device.get(), &swapchain};

    auto resizeSub = SubscribeEvent(
        events::WindowResized,
        [](void* user, const EventMeta&, EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowResizedPayload*>(
                  view.Data());
          (void)payload;
          auto* state = static_cast<ResizeState*>(user);
          state->Device->ResizeSwapchain(*state->SC);
        },
        &resizeState);

    auto keySub = SubscribeEvent(
        events::WindowKey,
        [](void* user, const EventMeta&, EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowKeyPayload*>(view.Data());
          if (payload->Down && !payload->Repeat &&
              payload->Key == KeyCode::Escape)
          {
            *static_cast<bool*>(user) = false;
          }
        },
        &running);

    GECKO_INFO(app::graphics_example::labels::Main,
               "Entering frame loop — press Escape or close window to quit");

    // ── Frame loop ────────────────────────────────────────────────
    while (running)
    {
      ctx.PumpEvents();
      (void)DispatchEvents();

      if (swapchain.IsValid() && pipeline.IsValid() && vertexBuffer.IsValid())
      {
        RenderTarget backBuffer = device->GetCurrentBackBuffer(swapchain);
        if (backBuffer.IsValid())
        {
          auto cmd = device->CreateGraphicsCommandList();
          cmd->Begin();

          cmd->BeginRendering(backBuffer, /*clearColor=*/true, /*clearDepth=*/false);

          cmd->SetViewport(0.0F, 0.0F,
                            static_cast<f32>(swapchain.Desc.Width),
                            static_cast<f32>(swapchain.Desc.Height),
                            0.0F, 1.0F);
          cmd->SetScissor(0, 0, swapchain.Desc.Width, swapchain.Desc.Height);

          cmd->BindPipeline(pipeline);
          cmd->BindVertexBuffer(vertexBuffer);
          cmd->DrawVertices(3, 1, 0, 0);

          cmd->EndRendering();
          cmd->End();

          device->ExecuteGraphicsCommandList(::std::move(cmd));
        }
      }

      device->Present(swapchain);
    }

    // ── Cleanup ───────────────────────────────────────────────────
    device->DestroySwapchain(swapchain);
    ctx.Windows().DestroyWindow(window);

    GECKO_INFO(app::graphics_example::labels::Main, "Shutdown complete");
  }

  GECKO_SHUTDOWN();
  ::std::printf("Graphics example finished.\n");
  return 0;
}
