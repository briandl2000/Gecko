#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

class ShadowPass : public RenderPass<ShadowPass>
{
public:
	struct ConfigData : public BaseConfigData
	{};

	ShadowPass() = default;
	virtual ~ShadowPass() {}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;


protected:
	friend class RenderPass<ShadowPass>;
	// Called by RenderPass<ShadowPass> to initialise this object with config data
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies);

private:
	GraphicsPipelineHandle ShadowPipelineHandle;

};

}