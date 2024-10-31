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
			PrevPass(RenderPassHandle())
		{}
		ConfigData(RenderPassHandle handle) :
			PrevPass(handle)
		{}
		
		RenderPassHandle PrevPass;
	};

	BloomPass() = default;
	virtual ~BloomPass() {}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;

protected:
	friend class RenderPass<BloomPass>;
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies);

private:
	
	BloomData m_BloomData;

	TextureHandle m_DownScaleTextureHandle;
	TextureHandle m_UpScaleTextureHandle;

	ComputePipelineHandle m_DownScalePipelineHandle;
	ComputePipelineHandle m_UpScalePipelineHandle;
	ComputePipelineHandle m_ThresholdPipelineHandle;
	ComputePipelineHandle m_CompositePipelineHandle;

	ConfigData m_ConfigData;

};

}