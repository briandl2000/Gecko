/// @file
/// `GraphicsFixture` -- engine + window + swapchain bootstrap helper.

#include <gecko/bench/graphics_fixture.h>
#include <gecko/core/labels.h>
#include <gecko/core/services/events.h>
#include <gecko/core/services/log.h>

namespace gecko::bench {

namespace {

constexpr ::gecko::Label kFixtureLabel =
    ::gecko::MakeLabel("gecko.bench.fixture");

::gecko::graphics::GraphicsConfig MakeGraphicsConfig(
    const GraphicsFixtureConfig& cfg)
{
  ::gecko::graphics::GraphicsConfig out {};
  out.Backend = ::gecko::graphics::GraphicsBackend::Vulkan;
  out.Debug = false;
  out.AppName = cfg.Title;
  return out;
}

}  // namespace

GraphicsFixture::GraphicsFixture(const GraphicsFixtureConfig& cfg) noexcept
    : m_GraphicsModule(MakeGraphicsConfig(cfg))
{
  if (!m_AllocScope)
    return;

  m_Engine =
      ::gecko::Engine::Create({&m_RuntimeModule, &m_PlatformModule,
                               &m_GraphicsModule, &m_DebugRendererModule});
  if (!m_Engine)
  {
    GECKO_ERROR(kFixtureLabel, "Engine::Create failed");
    return;
  }

  m_LogSinks.emplace();

  m_Device = ::gecko::graphics::GetGraphicsDevice();
  if (!m_Device)
  {
    GECKO_ERROR(kFixtureLabel, "GraphicsModule did not publish a device");
    return;
  }

  ::gecko::platform::WindowDesc wd;
  wd.Title = cfg.Title;
  wd.Size = {static_cast<::gecko::i32>(cfg.Width),
             static_cast<::gecko::i32>(cfg.Height)};
  wd.Visible = cfg.Visible;
  wd.Resizable = false;
  wd.Mode = ::gecko::platform::WindowMode::Windowed;
  m_Window = ::gecko::platform::GetWindows()->CreateWindow(wd);
  if (!m_Window.IsValid())
  {
    GECKO_ERROR(kFixtureLabel, "Failed to create window");
    return;
  }

  ::gecko::platform::NativeWindowHandle native =
      ::gecko::platform::GetWindows()->GetNativeWindowHandle(m_Window);
  ::gecko::platform::Extent2D sz =
      ::gecko::platform::GetWindows()->GetClientSize(m_Window);

  ::gecko::graphics::SwapchainDesc scDesc;
  scDesc.Width = sz.Width;
  scDesc.Height = sz.Height;
  scDesc.NumBackBuffers = cfg.NumBackBuffers;
  scDesc.Format = ::gecko::graphics::DataFormat::R8G8B8A8_UNORM;
  scDesc.VSync = cfg.VSync;
  m_Swapchain = m_Device->CreateSwapchain(native, scDesc);
  if (!m_Swapchain.IsValid())
  {
    GECKO_ERROR(kFixtureLabel, "Failed to create swapchain");
    return;
  }

  m_GpuSampler = ::gecko::graphics::GetGpuSampler();

  m_Valid = true;
}

GraphicsFixture::~GraphicsFixture() noexcept
{
  if (m_Device && m_Swapchain.IsValid())
    m_Device->DestroySwapchain(m_Swapchain);
  if (m_Engine && m_Window.IsValid())
    ::gecko::platform::GetWindows()->DestroyWindow(m_Window);
}

void GraphicsFixture::PumpEvents() noexcept
{
  ::gecko::platform::PumpEvents();
  (void)::gecko::DispatchEvents();
}

::gecko::graphics::FrameContext GraphicsFixture::BeginFrame() noexcept
{
  return m_Device->BeginFrame(m_Swapchain);
}

void GraphicsFixture::Present(::gecko::graphics::FrameContext& frame) noexcept
{
  ::gecko::graphics::FrameContext arr[1] = {::std::move(frame)};
  m_Device->Present(
      ::std::span<const ::gecko::graphics::FrameContext> {arr, 1});
}

}  // namespace gecko::bench
