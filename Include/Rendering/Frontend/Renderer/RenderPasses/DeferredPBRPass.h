#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"


namespace Gecko
{

struct PBRData
{
	u32 width;
	u32 height;
};

//struct DeferredPBRPass::InputData;
class DeferredPBRPass : public RenderPass<DeferredPBRPass>
{
public:
	struct Gecko::DeferredPBRPass::InputData : public BaseInputData
	{
		InputData() :
			GeoPass(RenderPassHandle()),
			ShadowPass(RenderPassHandle())
		{}
		InputData(RenderPassHandle geo, RenderPassHandle shadow) :
			GeoPass(geo),
			ShadowPass(shadow)
		{}
		RenderPassHandle GeoPass;
		RenderPassHandle ShadowPass;
	};

	DeferredPBRPass() = default;
	virtual ~DeferredPBRPass() {}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;

protected:
	friend class RenderPass<DeferredPBRPass>;
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const InputData& dependencies);

private:

	TextureHandle BRDFLUTTextureHandle;

	ComputePipelineHandle PBRPipelineHandle;

	InputData m_Input;
};

}