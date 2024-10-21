#include "ShaderDefines.h"

Texture2D_Arr<float4> InputTexture : register(t0);
RW_Texture2D_Arr<float4> OutputTexture : register(u0);
SamplerState PointSampler : register(s0);

struct FXAAData
{
	float fxaaSpanMax;
	float fxaaReduceMin;
	float fxaaReduceMul;
	float Scale;
	uint width;
	uint height;
};

DYNAMIC_CALL_DATA(FXAAData, fxaaData, b0);

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	
	float2 texelSize = 1. / float2(fxaaData.width, fxaaData.height);
	float2 uv = float2(DTid.xy + .5) / float2(fxaaData.width, fxaaData.height);

	float3 luma = float3(0.299, 0.587, 0.114);

	float3 TL = SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (float2(-1.0, -1.0) * texelSize), 0.).rgb;
	float3 TR = SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (float2( 1.0, -1.0) * texelSize), 0.).rgb;
	float3 BL = SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (float2(-1.0,  1.0) * texelSize), 0.).rgb;
	float3 BR = SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (float2( 1.0,  1.0) * texelSize), 0.).rgb;
	float3 M  = SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv									, 0.).rgb;

	float lumaTL = dot(luma, TL.xyz);
	float lumaTR = dot(luma, TR.xyz);
	float lumaBL = dot(luma, BL.xyz);
	float lumaBR = dot(luma, BR.xyz);
	float lumaM  = dot(luma, M.xyz);

	float2 dir;
	dir.x = -((lumaTL + lumaTR) - (lumaBL + lumaBR));
	dir.y =  (((lumaTL + lumaBL) - (lumaTR + lumaBR)));
	//OutputTexture[DTid] = float4(dir.x, dir.x, dir.x, 1.0);


	float dirReduce = max((lumaTL + lumaTR + lumaBL + lumaBR) * (fxaaData.fxaaReduceMul * 0.25), fxaaData.fxaaReduceMin);
	float inverseDirAdjustment = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

	/*dir = min(float2( fxaaData.fxaaSpanMax,  fxaaData.fxaaSpanMax),
		  max(float2(-fxaaData.fxaaSpanMax, -fxaaData.fxaaSpanMax), dir)) * texelSize;
	*/
	dir = clamp(dir * inverseDirAdjustment, -fxaaData.fxaaSpanMax, fxaaData.fxaaSpanMax) * texelSize;

	float3 result1 = (1.0 / 2.0) * (
		SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (dir * (1.0/3.0-0.5)), 0.).xyz +
		SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (dir * (2.0/3.0-0.5)), 0.).xyz);

	float3 result2 = result1 * (1.0 / 2.0) + (1.0 / 4.0) * (
		SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (dir * (0.0/3.0-0.5)), 0.).xyz +
		SAMPLE_TEXTURE2D_LEVEL(InputTexture, PointSampler, uv + (dir * (3.0/3.0-0.5)), 0.).xyz);

	float lumaMin = min(lumaM, min(min(lumaTL, lumaTR), min(lumaBL, lumaBR)));
	float lumaMax = max(lumaM, max(max(lumaTL, lumaTR), max(lumaBL, lumaBR)));
	float lumaResult2 = dot(luma, result2);

	if (lumaResult2 < lumaMin || lumaResult2 > lumaMax)
		OutputTexture[DTid] = float4(result1, 1.0);
	
	OutputTexture[DTid] = float4(result2, 1.0);
}
#endif