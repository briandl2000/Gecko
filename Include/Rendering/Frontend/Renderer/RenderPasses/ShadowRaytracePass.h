#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

	class ShadowRaytracePass : public RenderPass
	{
	public:
		ShadowRaytracePass() = default;
		virtual ~ShadowRaytracePass() {}

		virtual const void Init(Platform::AppInfo& appInfo, ResourceManager* resourceManager) override;
		virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager, Ref<CommandList> commandList) override;


	protected:

	private:
		RenderTargetHandle m_OutputHandle;

		RaytracingPipelineHandle m_ShadowRaytracePipelineHandle;
	};

}