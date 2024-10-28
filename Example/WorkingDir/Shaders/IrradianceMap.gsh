#include "ShaderDefines.h"

TextureCube InputTexture : register(t0);
RW_Texture2D_Arr<float4> OutputTexture : register(u0);
SamplerState LinearSampler : register(s0);

static const float PI = 3.141592;

float3 getSamplingfloattor(uint3 ThreadID)
{
	float outputWidth, outputHeight, outputDepth;
    OutputTexture.GetDimensions(outputWidth, outputHeight, outputDepth);

	float2 st = ThreadID.xy / float2(outputWidth, outputHeight);
	float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

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

#if defined (COMPUTE)
[N_THREADS(32, 32, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
    float3 N = getSamplingfloattor(DTid);

    float3 irradiance = float3(0.,0.,0.);

    // tangent space calculation from origin point
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float sampleDelta = 0.1;
    float nrSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += SAMPLE_TEXTURE_LEVEL(InputTexture, LinearSampler, sampleVec, 0).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));

    OutputTexture[DTid] = float4(irradiance, 1);
}
#endif