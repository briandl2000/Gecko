Texture2DArray<float4> InputTexture : register(t0);
RWTexture2DArray<float4> OutputTexture : register(u0);
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

ConstantBuffer<MipGenerationData> mipGenerationData : register(b0);

#if defined (COMPUTE)
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
  float3 uv = float3(float2(id.xy*2+1)/float2(mipGenerationData.width, mipGenerationData.height), id.z);

  float4 color = InputTexture.SampleLevel(LinearSampler, uv, 0.0);

  OutputTexture[id] = mipGenerationData.srgb ? linear_to_srgb(color) : color;
}
#endif
