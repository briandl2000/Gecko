// Animated plasma written to a storage image by a compute shader.
// Dispatched each frame; consumed as an SRV by the fullscreen blit pass.

[[vk::push_constant]]
cbuffer PC
{
    float Time;
    float _pad0;
    float _pad1;
    float _pad2;
};

[[vk::binding(0)]] RWTexture2D<float4> Output : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint w;
    uint h;
    Output.GetDimensions(w, h);
    if (tid.x >= w || tid.y >= h)
        return;

    float2 uv = (float2(tid.xy) + 0.5) / float2(w, h);
    float2 p  = uv * 2.0 - 1.0;

    float v = sin(p.x * 10.0 + Time)
            + sin(p.y * 10.0 + Time * 1.3)
            + sin((p.x + p.y) * 8.0 + Time * 0.7)
            + sin(length(p) * 12.0 - Time * 2.0);
    v *= 0.25;

    const float PI = 3.14159265;
    float3 col = 0.5 + 0.5 * float3(
        sin(v * PI + 0.0),
        sin(v * PI + 2.0),
        sin(v * PI + 4.0));

    Output[tid.xy] = float4(col, 1.0);
}
