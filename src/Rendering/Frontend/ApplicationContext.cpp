#include "Rendering/Frontend/ApplicationContext.h"

#include "Rendering/Backend/Device.h"

namespace Gecko
{

void ApplicationContext::Init(Platform::AppInfo& appInfo)
{
  m_Device = Gecko::Device::CreateDevice();
  m_Device->Init();

  m_ResourceManager.Init(m_Device.get());
  m_Renderer.Init(appInfo, &m_ResourceManager, m_Device.get());
  m_SceneManager.Init();
}

void ApplicationContext::Shutdown()
{
  m_SceneManager.Shutdown();
  m_Renderer.Shutdown();
  m_ResourceManager.Shutdown();

  m_Device->Shutdown();
  m_Device.reset();
}

}
