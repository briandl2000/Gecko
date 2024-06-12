#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{


struct FXAAData
{
	f32 fxaaSpanMax;
	f32 fxaaReduceMin;
	f32 fxaaReduceMul;
	f32 scale = 0.f;
	u32 width;
	u32 height;
};

class FXAAPass : public RenderPass
{
public:
	FXAAPass() = default;
	virtual ~FXAAPass() {}

	virtual const void Init(ResourceManager* resourceManager) override;
	virtual const void Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList) override;
protected:

private:

	FXAAData m_FXAAData;
	
	RenderTargetHandle m_OutputHandle;

	ComputePipelineHandle FXAAPipelineHandle;

};

}