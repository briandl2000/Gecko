#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

class ToneMappingGammaCorrectionPass : public RenderPass<ToneMappingGammaCorrectionPass>
{
public:
	struct ConfigData : public BaseConfigData
	{
		ConfigData() :
			PrevPassOutput(RenderTargetHandle())
		{}
		ConfigData(RenderTargetHandle handle) :
			PrevPassOutput(handle)
		{}
		
		RenderTargetHandle PrevPassOutput;
	};

	ToneMappingGammaCorrectionPass() = default;
	virtual ~ToneMappingGammaCorrectionPass() {}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;


protected:
	friend class RenderPass<ToneMappingGammaCorrectionPass>;
	// Called by RenderPass<ToneMappingGammaCorrectionPass> to initialise this object with config data
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies);

private:
	ComputePipelineHandle TonemapAndGammaCorrectPipelineHandle;
};

}