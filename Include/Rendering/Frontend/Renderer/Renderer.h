#pragma once
#include <unordered_map>

#include "Defines.h"

#include "Rendering/Backend/Objects.h"
#include "Rendering/Backend/Device.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

#include "Core/Platform.h"


namespace Gecko {


struct SceneDescriptor;

// Used in template function in this header, so has to be outside of class scope
static unsigned int s_NumRenderPasses = 0;

class Renderer
{
public:
	Renderer() = default;
	~Renderer();
	
	void Init(Platform::AppInfo& info, ResourceManager* resourceManager, Device* _device);
	void Shutdown();

	template<typename T>
	RenderPassHandle CreateRenderPass(std::string name,
		const RenderPassInterface::ConfigDataInterface& dependencies = RenderPassInterface::ConfigDataInterface())
	{
		Ref<T> renderPass = CreateRef<T>();
		renderPass->Init(m_Info, m_ResourceManager, dependencies);
		RenderPassHandle handle = name;
		m_RenderPasses.emplace(std::make_pair(handle, renderPass));
		return handle;
	}

	const Ref<RenderPassInterface>& GetRenderPassByHandle(RenderPassHandle id) const
	{
		return m_RenderPasses.at(id);
	}

	void ConfigureRenderPasses(const std::vector<RenderPassHandle>& renderPassStack)
	{
		m_RenderPassStack = renderPassStack;
	}

	void RenderScene(const SceneRenderInfo& sceneRenderInfo);
	void Present();

private:

	Device* device;

	ResourceManager* m_ResourceManager;

	std::unordered_map<RenderPassHandle, Ref<RenderPassInterface>> m_RenderPasses;
	std::vector<RenderPassHandle> m_RenderPassStack;

	GraphicsPipelineHandle FullScreenTexturePipelineHandle;

	MeshHandle quadMeshHandle{ 0 };

	Platform::AppInfo m_Info;
};

}
