#include "ShaderDefines.h"

RW_Texture2D_Arr<float4> Input : register(u0);
RW_Texture2D_Arr<float4> Output : register(u1);

ConstantBuffer<SceneData> sd : register(b0);

float3 aces_approx(float3 v)
{
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v * (a * v + b)) / (v * (c * v + d) + e), 0.0f, 1.0f);
}

float3 Uncharted2ToneMapping(float3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	color *= sd.exposure;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	return color;
}

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	float3 color = Input[DTid].rgb;

    color = Uncharted2ToneMapping(color);
	float Gamma = 1. / 2.2;
	color = pow(abs(color), Gamma);

	Output[DTid] = float4(color, 1.);
}
#endif