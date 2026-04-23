// Fullscreen blit: samples the offscreen RT and writes to the backbuffer.
// Compiled with: glslc -x hlsl -fshader-stage=frag -fentry-point=main
//
// Texture2D and SamplerState map to separate Vulkan descriptors
// (VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE at binding 0 and
// VK_DESCRIPTOR_TYPE_SAMPLER at binding 1), matching the pipeline layout
// produced by VulkanDevice for a TextureBinding + SamplerBinding pair.

[[vk::binding(0, 0)]] Texture2D    g_Source  : register(t0);
[[vk::binding(1, 0)]] SamplerState g_Sampler : register(s0);

struct PushConstants
{
    float4 Tint;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return g_Source.Sample(g_Sampler, input.UV) * g_Push.Tint;
}
