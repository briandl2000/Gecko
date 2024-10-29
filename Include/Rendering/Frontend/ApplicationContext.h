#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/Renderer.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Scene/SceneManager.h"
#include "Core/Platform.h"
#include "Core/Logger.h"


namespace Gecko
{

class Device;

class ApplicationContext
{
public:
	ApplicationContext() = default;
	~ApplicationContext() {};

	void Init(Platform::AppInfo& appInfo);
	void Shutdown();

	ResourceManager* GetResourceManager() { return &m_ResourceManager; }
	Renderer* GetRenderer() { return &m_Renderer; }
	SceneManager* GetSceneManager() { return &m_SceneManager; }

private:
	
	Ref<Device> m_Device;

	ResourceManager m_ResourceManager;
	Renderer m_Renderer;
	SceneManager m_SceneManager;

};

}