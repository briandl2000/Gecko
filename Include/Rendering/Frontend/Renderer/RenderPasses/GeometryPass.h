#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{



class GeometryPass : public RenderPass<GeometryPass>
{
public:
	struct ConfigData : public BaseConfigData
	{};

	GeometryPass() = default;
	virtual ~GeometryPass() {}
	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override;


protected:
	friend class RenderPass<GeometryPass>;
	// Called by RenderPass<GeometryPass> to initialise this object with config data
	virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies);

private:
	GraphicsPipelineHandle CubemapPipelineHandle;
	GraphicsPipelineHandle GBufferPipelineHandle;
};

}