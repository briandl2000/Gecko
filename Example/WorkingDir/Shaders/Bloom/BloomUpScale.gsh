#include "../ShaderDefines.h"

Texture2D<float4> OutPutSmallTexture : register(t0);
RW_Texture2D_Arr<float4> InputTexture : register(u0);
RW_Texture2D_Arr<float4> OutputTexture : register(u1);
SamplerState LinearSampler : register(s0);

struct BloomData
{
	uint Width;
	uint Height;
	float Threshold;
};

DYNAMIC_CALL_DATA(BloomData, bloomData, b0);

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	float2 texelSize = 1.f / float2(bloomData.Width, bloomData.Height);
	float2 uv = float2(DTid.xy + .5f) / float2(bloomData.Width, bloomData.Height);

	float4 color = UpsampleTent(OutPutSmallTexture, LinearSampler, uv, texelSize, float4(1.f, 1.f, 1.f, 1.f));

	OutputTexture[DTid] = InputTexture[DTid] + color;
}
#endif