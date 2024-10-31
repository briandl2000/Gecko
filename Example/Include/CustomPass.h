#pragma once

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

class CustomPass : public Gecko::RenderPass<CustomPass>
{
public:
	struct ConfigData : public Gecko::BaseConfigData
	{

	};

	virtual const void Render(const Gecko::SceneRenderInfo& sceneRenderInfo, Gecko::ResourceManager* resourceManager,
		const Gecko::Renderer* renderer, Gecko::Ref<Gecko::CommandList> commandList) override;

protected:
	friend class Gecko::RenderPass<CustomPass>;
	virtual const void SubInit(const Gecko::Platform::AppInfo& appInfo, Gecko::ResourceManager* resourceManager, 
		const ConfigData& dependencies);

private:
	Gecko::RenderTargetHandle m_OutputHandle;

	Gecko::ComputePipelineHandle CustomPipelineHandle;
};