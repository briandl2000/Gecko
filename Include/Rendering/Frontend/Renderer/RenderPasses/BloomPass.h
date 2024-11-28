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

class BloomPass : public RenderPass<BloomPass>
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

	BloomPass() = default;
	virtual ~BloomPass() {}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;

protected:
	friend class RenderPass<BloomPass>;
	// Called by RenderPass<BloomPass> to initialise this object with config data
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies);

private:
	
	BloomData m_BloomData;

	RenderTargetHandle m_DownScaleRenderTargetHandle;
	RenderTargetHandle m_UpScaleRenderTargetHandle;

	ComputePipelineHandle m_DownScalePipelineHandle;
	ComputePipelineHandle m_UpScalePipelineHandle;
	ComputePipelineHandle m_ThresholdPipelineHandle;
	ComputePipelineHandle m_CompositePipelineHandle;
};

}