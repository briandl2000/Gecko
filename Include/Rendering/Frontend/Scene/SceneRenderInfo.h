#pragma once

#include "Defines.h"

#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"

#include "glm/common.hpp"

namespace Gecko {

	
	struct Material
	{
		u32 materialIndex{ 0 };
		TextureHandle AlbedoTextureHandle{ 0 };
		TextureHandle NormalTextureHandle{ 0 };
		TextureHandle MetalicRoughnessTextureHandle{ 0 };
		TextureHandle OcclusionTextureHandle{ 0 };
		TextureHandle EmmisiveTextureHandle{ 0 };
	};

	struct RenderObjectRenderInfo
	{
		MeshHandle MeshHandle{ 0 };
		Material Material;
		glm::mat4 Transform{ 0.f };
	};

	struct CameraRenderInfo
	{
		glm::mat4 View{ 0.f };
		glm::mat4 Projection{ 0.f };
	};

	struct DirectionalLightRenderInfo
	{
		glm::vec3 Color{ 0.f };
		f32 Intensity{ 0.f };
		glm::mat4 Transform{ 0.f };
	};

	struct PointLightRenderInfo
	{
		glm::vec3 Color{ 0.f };
		f32 Intensity{ 0.f };
		glm::mat4 Transform{ 0.f };
		f32 Radius{ 0.f };
	};

	struct SpotLightRenderInfo
	{
		glm::vec3 Color{ 0.f };
		f32 Intensity{ 0.f };
		glm::mat4 Transform{ 0.f };
		f32 Radius{ 0.f };
		f32 SpotAngle{ 0.f };
	};

	struct SceneRenderInfo
	{
		std::vector<RenderObjectRenderInfo> RenderObjects;
		std::vector<DirectionalLightRenderInfo> DirectionalLights;
		std::vector<PointLightRenderInfo> PointLights;
		std::vector<SpotLightRenderInfo> SpotLights;

		bool HasCamera{ false };
		CameraRenderInfo Camera;
	
		EnvironmentMapHandle EnvironmentMap{ 0 };
	};

}