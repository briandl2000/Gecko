#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

namespace Gecko
{

	class ShadowRaytracePass : public RenderPass<ShadowRaytracePass>
	{
	public:
		struct ConfigData : public BaseConfigData
		{
			ConfigData() :
				GeoPass(RenderPassHandle())
			{}
			ConfigData(RenderPassHandle handle) :
				GeoPass(handle)
			{}
			
			RenderPassHandle GeoPass;
		};

		ShadowRaytracePass() = default;
		virtual ~ShadowRaytracePass() {}

		virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
			const Renderer* renderer, Ref<CommandList> commandList) override;


	protected:
		friend class RenderPass<ShadowRaytracePass>;
		virtual const void SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies);

	private:
		RaytracingPipelineHandle m_ShadowRaytracePipelineHandle;

		ConfigData m_ConfigData;
	};

}