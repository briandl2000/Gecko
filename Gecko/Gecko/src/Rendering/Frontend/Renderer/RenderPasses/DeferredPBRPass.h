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

class DeferredPBRPass : public RenderPass
{
public:
	DeferredPBRPass() = default;
	virtual ~DeferredPBRPass() {}

	virtual const void Init(ResourceManager* resourceManager) override;
	virtual const void Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList) override;
protected:

private:

	TextureHandle BRDFLUTTextureHandle;
	RenderTargetHandle PBROutputHandle;

	ComputePipelineHandle PBRPipelineHandle;

};

}