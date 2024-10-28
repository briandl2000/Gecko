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
};

struct ModelData
{
	float4x4 modelTransform;
};

ConstantBuffer<SceneData> sd : register(b0);

DYNAMIC_CALL_DATA(ModelData, modelData, b1);

#if defined (VERTEX)
VS_OUTPUT main(VS_INPUT vertex)
{
	VS_OUTPUT Output;

	float4x4 modelMat = modelData.modelTransform;

	float4x4 mvp = mul(sd.ShadowMapProjection, modelMat);

	Output.Position = mul(mvp, float4(vertex.position, 1.));
	
	return Output;
}
#elif defined (PIXEL)

struct PS_Output
{
	float4 temp : S_OUTPUT_TARGET0;
	float depth : S_OUTPUT_DEPTH;
};

PS_Output main(VS_OUTPUT input)
{
	PS_Output output;
	output.temp = float4(0., 0., 0., 0.);
	output.depth = (input.Position.z / input.Position.w)*.5+.5;
	
	if (output.depth < .5 || output.depth > 1.)
	{
		discard;
	}
	return output;
}
#endif