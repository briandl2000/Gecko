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

		virtual const void Init(ResourceManager* resourceManager) override;
		virtual const void Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList) override;
	protected:

	private:
		RenderTargetHandle m_OutputHandle;

		RaytracingPipelineHandle m_ShadowRaytracePipelineHandle;
	};

}