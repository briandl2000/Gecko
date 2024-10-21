#include "ShaderDefines.h"

Texture2D_Arr<float4> inTexture : register(t0);
RW_Texture2D_Arr<float4> output : register(u0);
SamplerState smp : register(s0);

float3 getSamplingVector(uint3 ThreadID)
{
	float outputWidth, outputHeight, outputDepth;
	output.GetDimensions(outputWidth, outputHeight, outputDepth);

	float2 st = ThreadID.xy / float2(outputWidth, outputHeight);
	float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

	// Select vector based on cubemap face index.
	float3 ret = float3(0, 0, 0);
	switch (ThreadID.z)
	{
	case 0: ret = float3(1.0, uv.y, -uv.x); break;
	case 1: ret = float3(-1.0, uv.y, uv.x); break;
	case 2: ret = float3(uv.x, 1.0, -uv.y); break;
	case 3: ret = float3(uv.x, -1.0, uv.y); break;
	case 4: ret = float3(uv.x, uv.y, 1.0); break;
	case 5: ret = float3(-uv.x, uv.y, -1.0); break;
	}
	return normalize(ret);
}

float2 DirectionToUV(float3 direction)
{
	float theta = acos(direction.y);
	float phi = atan2(direction.z, direction.x);

	float u = phi / (2 * 3.14159265359) + 0.5;
	float v = (theta / 3.14159265359);

	return float2(u, v);
}

#if defined (COMPUTE)
[N_THREADS(32, 32, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	float3 sampleingVector = getSamplingVector(DTid);
	
	
	float3 uv = float3(DirectionToUV(sampleingVector), 0);

	float inputWidth, inputHeight, inputDepth;
	inTexture.GetDimensions(inputWidth, inputHeight, inputDepth);

	float3 subPixelSize = float3(1./float2(inputWidth, inputHeight), 1.);



	float3 color = (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3(-1., -1., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3(-1.,  0., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3(-1.,  1., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3( 0., -1., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3( 0.,  0., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3( 0.,  1., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3( 1., -1., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3( 1.,  0., 0.), 0.0f)).xyz * 1. + 
				   (SAMPLE_TEXTURE_LEVEL(inTexture, smp, uv + subPixelSize*float3( 1.,  1., 0.), 0.0f)).xyz * 1. ; 


	color = min(color, 100.);
	color /= 9.;
	
	output[DTid] = float4(color, 1);

}
#endif