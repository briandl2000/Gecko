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

class FXAAPass : public RenderPass<FXAAPass>
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

	FXAAPass() = default;
	virtual ~FXAAPass() {}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;

protected:
	friend class RenderPass<FXAAPass>;
	// Called by RenderPass<FXAAPass> to initialise this object with config data
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies);

private:

	FXAAData m_FXAAData;

	ComputePipelineHandle FXAAPipelineHandle;

	ConfigData m_ConfigData;

};

}