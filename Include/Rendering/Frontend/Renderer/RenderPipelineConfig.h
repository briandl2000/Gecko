#pragma once
#include <vector>
#include <map>
#include "RenderPass.h"

namespace Gecko
{
	/* 
	*	Wrapper class for one or more render passes that either do not depend on each other or
	*	depend on each other linearly (each pass uses the output of the previous pass as its only input).
	*	Contains functionality for configuring dependencies on other render passes or pipeline configs.
	*/
	class RenderPipelineConfig
	{
	public:
		RenderPipelineConfig();
		RenderPipelineConfig(const std::vector<RenderPass>& a_Passes, std::vector<RenderPassID>& r_Keys);
		RenderPipelineConfig(const std::vector<RenderPipelineConfig>& a_Configs);
		virtual ~RenderPipelineConfig();

		std::map<RenderPassID, RenderTargetHandle> GetRenderPassOutputs();
	private:
		std::map<RenderPassID, RenderPass> m_RenderPasses;
		std::vector<RenderPipelineConfig> m_SubConfigs;
		std::vector<RenderPipelineConfig> m_Dependencies;
	};
}