#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

struct BloomData
{
	u32 Width;
	u32 Height;
	f32 Threshold;
};

class BloomPass : public RenderPass
{
public:
	BloomPass() = default;
	virtual ~BloomPass() {}

	virtual const void Init(ResourceManager* resourceManager) override;
	virtual const void Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList) override;
protected:

private:
	
	BloomData m_BloomData;

	RenderTargetHandle m_OutputTargetHandle;
	TextureHandle m_DownScaleTextureHandle;
	TextureHandle m_UpScaleTextureHandle;

	ComputePipelineHandle m_DownScalePipelineHandle;
	ComputePipelineHandle m_UpScalePipelineHandle;
	ComputePipelineHandle m_ThresholdPipelineHandle;
	ComputePipelineHandle m_CompositePipelineHandle;

};

}