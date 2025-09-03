struct VS_INPUT
{
	float3 position : POSITION;
	float3 color : COLOR;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
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

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return float4(input.Color, 0.0);
}
#endif