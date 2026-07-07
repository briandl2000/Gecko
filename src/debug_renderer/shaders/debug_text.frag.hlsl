[[vk::binding(2, 0)]] Texture2D g_Source  : register(t2);
[[vk::binding(3, 0)]] SamplerState g_Sampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV    : TexCoord;
    float3 Color : COLOR;
};

float4 main(PSInput input) : SV_Target
{
  float r = g_Source.Sample(g_Sampler, input.UV).r;
  if (r < 0.5)
    discard;
  return float4(input.Color, 1.0);
}
