#include "ShaderDefines.h"

Texture2D_Arr<float4> InputTexture : register(t0);
RW_Texture2D_Arr<float4> OutputTexture : register(u0);
SamplerState LinearSampler : register(s0);

struct MipGenerationData
{
	uint baseMip;
	uint srgb;
	uint width;
	uint height;
};

float4 linear_to_srgb(float4 col)
{
	float3 x = col.rgb * 12.92f;
	float3 y = 1.055f * pow(abs(col.rgb), 1.0f / 2.4f) - 0.055f;

	float4 r;
	r.r = col.r < 0.0031308f ? x.r : y.r;
	r.g = col.g < 0.0031308f ? x.g : y.g;
	r.b = col.b < 0.0031308f ? x.b : y.b;
	r.a = col.a;

	return r;
}

DYNAMIC_CALL_DATA(MipGenerationData, mipGenerationData, b0);

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	float3 uv = float3(float2(DTid.xy*2+1)/float2(mipGenerationData.width, mipGenerationData.height), DTid.z);

	float4 color = SAMPLE_TEXTURE_LEVEL(InputTexture, LinearSampler, uv, 0.);

	OutputTexture[DTid] = mipGenerationData.srgb ? linear_to_srgb(color) : color;
}
#endif