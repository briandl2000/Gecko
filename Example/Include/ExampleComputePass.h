#pragma once

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

class ExampleComputePass : public Gecko::RenderPass<ExampleComputePass>
{
public:
	// Struct ConfigData needs to be defined and needs to inherit from BaseConfigData. Any input dependencies, as well as
	// other optional user-defined settings, should be defined in this struct
	struct ConfigData : public Gecko::BaseConfigData
	{

	};

	virtual const void Render(const Gecko::SceneRenderInfo& sceneRenderInfo, Gecko::ResourceManager* resourceManager,
		const Gecko::Renderer* renderer, Gecko::Ref<Gecko::CommandList> commandList) override;

protected:
	// Friend class declaration is needed to give RenderPass access to SubInit() function
	friend class Gecko::RenderPass<ExampleComputePass>;

	// SubInit() needs to be defined; it is called by RenderPass. Initialisation happens in this function (RenderPass::Init() 
	// is declared final and calls Derived::SubInit())
	virtual const void SubInit(const Gecko::Platform::AppInfo& appInfo, Gecko::ResourceManager* resourceManager, 
		const ConfigData& dependencies);

private:
	// The compute pipeline used to render to the output target, initialised in SubInit()
	Gecko::ComputePipelineHandle m_ExamplePipelineHandle{ static_cast<Gecko::u32>(-1) };
};