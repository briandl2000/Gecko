#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

class ToneMappingGammaCorrectionPass : public RenderPass<ToneMappingGammaCorrectionPass>
{
public:
	struct InputData : public BaseInputData
	{
		RenderPassHandle PrevPass;

		InputData() : PrevPass(RenderPassHandle())
		{}
		InputData(RenderPassHandle handle) : PrevPass(handle)
		{}
	};

	ToneMappingGammaCorrectionPass() = default;
	virtual ~ToneMappingGammaCorrectionPass() {}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;


protected:
	friend class RenderPass<ToneMappingGammaCorrectionPass>;
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const InputData& dependencies);

private:
	ComputePipelineHandle TonemapAndGammaCorrectPipelineHandle;

	InputData m_Input;
};

}