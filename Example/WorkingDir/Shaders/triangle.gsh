#include "ShaderDefines.h"

struct VS_INPUT
{
	float3 position : POSITION;
	float3 color : COLOR;
};

struct VS_OUTPUT
{
	float4 Position : S_OUTPUT_POSITION;
	float3 Color : COLOR;
};

#if defined(VERTEX)
VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;

	Output.Position = float4(Input.position, 1.0);
	Output.Color = Input.color;

	return Output;
}
#elif defined(PIXEL)
struct PS_Output
{
	float4 Albedo   : S_OUTPUT_TARGET0;
};

PS_Output main(VS_OUTPUT input)
{
	PS_Output output;

	output.Albedo = float4(input.Color, 0.0);

	return output;
}
#endif