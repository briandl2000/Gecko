#include "Rendering/Frontend/Context.h"

#include "Rendering/Backend/Device.h"

namespace Gecko
{

void Context::Init(Platform::AppInfo& appInfo)
{
	m_Device = Gecko::Device::CreateDevice();

	m_ResourceManager.Init(m_Device.get());
	m_Renderer.Init(appInfo, &m_ResourceManager, m_Device.get());
	m_SceneManager.Init();
}

void Context::Shutdown()
{
	m_SceneManager.Shutdown();
	m_Renderer.Shutdown();
	m_ResourceManager.Shutdown();

	m_Device->Destroy();
	m_Device.reset();
}

}