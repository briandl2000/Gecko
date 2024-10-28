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
	float3 TextureCoord : TEXCOORD;
};

ConstantBuffer<SceneData> sceneData : register(b0);

TextureCube<float4> CubeMap : register(t0);
SamplerState Sampler : register(s0);

#if defined(VERTEX)
VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;

	float4 pos = mul(mul(sceneData.projection, sceneData.invViewOrientation), float4(Input.position.xyz, 1.f));
	Output.TextureCoord = Input.position;
	Output.Position = pos.xyww;

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
	PS_Output output;

	float3 color = CubeMap.Sample(Sampler, input.TextureCoord).xyz;
	float3 directionalLight = smoothstep(
			0.999f, 
			0.99999f, 
			dot(normalize(input.TextureCoord), -sceneData.LightDirection)
	)* sceneData.LighIntensity * sceneData.LightColor;
	output.Albedo = float4(0.f, 0.f, 0.f, 1.f);
	output.Normal = float4(0.f, 0.f, 0.f, 1.f);
	output.Position = float4(0.f, 0.f, 0.f, 1.f);
	output.Emissive = float4(color * sceneData.ambientIntensity + directionalLight, 1.);
	output.MRO = float4(0.f, 0.f, 0.f, 1.f);

	return output;
}
#endif