#pragma once

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

class CustomPass : public Gecko::RenderPass
{
public:
	virtual const void Init(Gecko::Platform::AppInfo& appInfo, Gecko::ResourceManager* resourceManager) override;
	virtual const void Render(const Gecko::SceneRenderInfo& sceneRenderInfo, Gecko::ResourceManager* resourceManager,
		Gecko::Ref<Gecko::CommandList> commandList) override;

private:
	Gecko::RenderTargetHandle m_OutputHandle;

	Gecko::ComputePipelineHandle CustomPipelineHandle;
};