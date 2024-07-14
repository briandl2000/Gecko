#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{



class GeometryPass : public RenderPass
{
public:
	GeometryPass() = default;
	virtual ~GeometryPass() {}

	virtual const void Init(Platform::AppInfo& appInfo, ResourceManager* resourceManager) override;
	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager, Ref<CommandList> commandList) override;


protected:

private:

	RenderTargetHandle m_OutputHandle;

	GraphicsPipelineHandle CubemapPipelineHandle;
	GraphicsPipelineHandle GBufferPipelineHandle;

};

}