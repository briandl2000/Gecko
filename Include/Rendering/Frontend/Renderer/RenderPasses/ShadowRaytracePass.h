#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

	class ShadowRaytracePass : public RenderPass<ShadowRaytracePass>
	{
	public:
		struct InputData : public BaseInputData
		{
			RenderPassHandle GeoPass;

			InputData() : GeoPass(RenderPassHandle())
			{}
			InputData(RenderPassHandle handle) : GeoPass(handle)
			{}
		};

		ShadowRaytracePass() = default;
		virtual ~ShadowRaytracePass() {}

		virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
			const Renderer* renderer, Ref<CommandList> commandList) override;


	protected:
		friend class RenderPass<ShadowRaytracePass>;
		virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const InputData& dependencies);

	private:
		RaytracingPipelineHandle m_ShadowRaytracePipelineHandle;

		InputData m_Input;
	};

}