// Fullscreen blit: samples the offscreen RT and writes to the backbuffer.
// Compiled with: glslc -x hlsl -fshader-stage=frag -fentry-point=main
//                       -fauto-combined-image-sampler
//
// The -fauto-combined-image-sampler flag makes glslc fuse the Texture2D
// and SamplerState declarations into a single VK_DESCRIPTOR_TYPE_
// COMBINED_IMAGE_SAMPLER binding (set=0, binding=0), matching the
// pipeline layout produced by VulkanDevice for a single Texture resource
// with an immutable sampler.

Texture2D    g_Source  : register(t0);
SamplerState g_Sampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return g_Source.Sample(g_Sampler, input.UV);
}
