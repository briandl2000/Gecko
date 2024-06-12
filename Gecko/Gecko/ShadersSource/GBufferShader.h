#include "ShaderDefines.h"

struct VS_INPUT
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
};

struct VS_OUTPUT
{
	float4 Position : S_OUTPUT_POSITION;
	float3 WorldPosition : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3x3 TBN : TBN;
	float2 uv : TEXCOORD;
};

struct ModelData
{
	float4x4 modelTransform;
};

#define DIFFUSE_MAP_FLAG 1
#define NORMAL_MAP_FLAG 2
#define MR_MAP_FLAG 4
#define EMMISIVE_MAP_FLAG 8
#define OCCLUSION_MAP_FLAG 16

struct Material {
	float4 baseColorFactor;
	float matallicFactor;
	float roughnessFactor;

	float normalScale;

	float occlusionStrength;

	float4 emissiveFactor;

	uint materialMaps;
};

ConstantBuffer<SceneData> sd : register(b0);
ConstantBuffer<Material> material : register(b1);

DYNAMIC_CALL_DATA(ModelData, modelData, b2);

Texture2D diffuseTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D MRTex : register(t2);
Texture2D EmmisiveTex : register(t3);
Texture2D OcclusionTex : register(t4);
SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);


#if defined (VERTEX)
VS_OUTPUT main(VS_INPUT vertex)
{
	VS_OUTPUT Output;

	float4x4 modelMat = modelData.modelTransform;

	float4x4 mvp = mul(mul(sd.projection, sd.invView), modelMat);
	
	float3 normal = normalize(mul(modelMat, float4(normalize(vertex.normal), 0.))).xyz;
	float3 tangent = normalize(mul(modelMat, float4(normalize(vertex.tangent.xyz), 0.))).xyz;
	float3 bitangent = cross(normal, tangent);
	bitangent = normalize(bitangent) * vertex.tangent.w;

	Output.Position = mul(mvp, float4(vertex.position, 1.));
	Output.WorldPosition = mul(modelMat, float4(vertex.position, 1.)).xyz;
	Output.normal = normal;
	Output.tangent = tangent;
	Output.TBN = float3x3(tangent, bitangent, normal);
	Output.uv = vertex.uv;

	return Output;
}

#elif defined(PIXEL)

struct PS_Output
{
	float4 Albedo   : S_OUTPUT_TARGET0;
	float4 Normal   : S_OUTPUT_TARGET1;
	float4 Position : S_OUTPUT_TARGET2;
	float4 Emissive : S_OUTPUT_TARGET3;
	float4 MRO      : S_OUTPUT_TARGET4;
};

PS_Output main(VS_OUTPUT input)
{


	float2 sampleUV = input.uv;

	float4 albedo = material.baseColorFactor;
	if (material.materialMaps & DIFFUSE_MAP_FLAG) {
		float4 s = diffuseTex.Sample(LinearSampler, sampleUV);
		albedo *= s;
	}
	if (albedo.w < 0.01) discard;

	float3 normal = input.normal;
	if (material.materialMaps & NORMAL_MAP_FLAG) {
		float3 normalSample = normalTex.Sample(LinearSampler, sampleUV).xyz;
		normal = normalize(normalSample * 2. - 1.);
		normal *= float3(material.normalScale, material.normalScale, 1.);
		normal = normalize(normal);
		normal = normalize(mul(normal, input.TBN));
	}

	float3 emmisive = material.emissiveFactor.xyz * material.emissiveFactor.w;
	if (material.materialMaps & EMMISIVE_MAP_FLAG)
	{
		emmisive = EmmisiveTex.Sample(LinearSampler, sampleUV).xyz;
		emmisive *= material.emissiveFactor.xyz * material.emissiveFactor.w;
	}

	float metalness = material.matallicFactor;
	float roughness = material.roughnessFactor;

	if (material.materialMaps & MR_MAP_FLAG) {
		float4 mr = MRTex.Sample(PointSampler, sampleUV);
		metalness *= mr.b;
		roughness *= mr.g;
	}
	float ao = 1.;
	if (material.materialMaps & OCCLUSION_MAP_FLAG) {
		float occlusion = OcclusionTex.Sample(PointSampler, sampleUV).r;
		ao *= occlusion * material.occlusionStrength;
	}

	PS_Output output;

	output.Albedo	= float4(albedo.rgb, 1.);
	output.Normal	= float4(normal, 1.);
	output.Position = float4(input.WorldPosition, 1.);
	output.Emissive	= float4(emmisive, 1.);
	output.MRO		= float4(metalness, roughness, ao, 1.);

	return output;
}
#endif