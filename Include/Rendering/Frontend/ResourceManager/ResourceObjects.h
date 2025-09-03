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
		Buffer VertexBuffer;
		Buffer IndexBuffer;
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
		Buffer MaterialConstantBuffer;
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

	struct RenderTargetResource
	{
		RenderTarget RenderTarget;
		bool KeepWindowAspectRatio{ false };
		f32 WidthScale{ 1.f };
	};

}