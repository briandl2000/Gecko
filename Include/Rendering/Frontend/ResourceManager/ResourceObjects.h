#pragma once

#include "Defines.h"

#include "Rendering/Backend/Objects.h"

namespace Gecko
{

	using MeshHandle = u32;
	using TextureHandle = u32;
	using MaterialHandle = u32;
	using RenderTargetHandle = u32;
	using EnvironmentMapHandle = u32;
	using GraphicsPipelineHandle = u32;
	using ComputePipelineHandle = u32;
	using RaytracingPipelineHandle = u32;

	struct Mesh
	{
		VertexBuffer VertexBuffer;
		IndexBuffer IndexBuffer;
		bool HasBLAS{ false };
		BLAS BLAS;
	};

	enum class MaterialTextureFlags : u32
	{
		Albedo				= BIT(0),
		Normal				= BIT(1),
		MetalicRoughness	= BIT(2),
		Emmisive			= BIT(3),
		Occlusion			= BIT(4),
	};

	struct MaterialData {
		f32 baseColorFactor[4] = { 1., 1., 1., 1. };
		f32 matallicFactor = 1.;
		f32 roughnessFactor = 1.;
		f32 normalScale = 1.;
		f32 occlusionStrength = 1.;
		f32 emissiveFactor[4] = { 1., 1., 1., 1. };
		u32 materialTextureFlags = 0x0000;
	};

	struct Material
	{
		ConstantBuffer MaterialConstantBuffer;
		TextureHandle AlbedoTextureHandle;
		TextureHandle NormalTextureHandle;
		TextureHandle MetalicRoughnessTextureHandle;
		TextureHandle OcclusionTextureHandle;
		TextureHandle EmmisiveTextureHandle;
	};

	struct EnvironmentMap
	{
		TextureHandle EnvironmentTextureHandle;
		TextureHandle IrradianceTextureHandle;
		TextureHandle HDRTextureHandle;
	};


	// TODO: figure out where this goes
	struct Vertex3D
	{
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
		glm::vec4 tangent;

		static const VertexLayout GetLayout() {
			VertexLayout layout({
				{ DataFormat::R32G32B32_FLOAT, "POSITION"},
				{ DataFormat::R32G32_FLOAT, "TEXCOORD" },
				{ DataFormat::R32G32B32_FLOAT, "NORMAL" },
				{ DataFormat::R32G32B32A32_FLOAT, "TANGENT" }
			});
			return layout;
		}
	};

	struct SceneDataStruct
	{
		glm::mat4 ViewMatrix;
		glm::mat4 InvViewMatrix;
		glm::mat4 ProjectionMatrix;
		glm::mat4 invProjectionMatrix;
		glm::mat4 ViewOrientation;
		glm::mat4 InvViewOrientation;
		glm::mat4 ShadowMapProjection;
		glm::mat4 ShadowMapProjectionInv;
		glm::vec3 CameraPosition;
		f32 LighIntensity;
		glm::vec3 LightDirection;
		f32 Exposure = .5f;
		glm::vec3 LightColor;
		f32 AmbientIntensity;
		f32 useRaytracing;
	};

	struct RenderTargetResource
	{
		RenderTarget RenderTarget;
		bool KeepWindowAspectRatio{ false };
		f32 WidthScale{ 1.f };
	};

}