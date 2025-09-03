
struct VS_INPUT
{
	float2 Position : POSITION0;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D inputTexture : register(t0);
SamplerState LinearSampler : register(s0);

#if defined(VERTEX)
VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;

	Output.Position = float4(Input.Position, 0., 1.);
	Output.uv = (Input.Position.xy/2.+.5)*float2(1., -1.);

	return Output;
}
#elif defined(PIXEL)
float4 main(VS_OUTPUT input) : SV_TARGET
{
	return float4(inputTexture.Sample(LinearSampler, input.uv).xyz, 1.);
}
#endif