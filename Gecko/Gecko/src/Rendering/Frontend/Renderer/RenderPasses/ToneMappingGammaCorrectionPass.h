#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

class ToneMappingGammaCorrectionPass : public RenderPass
{
public:
	ToneMappingGammaCorrectionPass() = default;
	virtual ~ToneMappingGammaCorrectionPass() {}

	virtual const void Init(ResourceManager* resourceManager) override;
	virtual const void Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList) override;
protected:

private:

	RenderTargetHandle m_OutputHandle;

	ComputePipelineHandle TonemapAndGammaCorrectPipelineHandle;

};

}