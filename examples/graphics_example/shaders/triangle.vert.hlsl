// Vertex shader for the coloured triangle drawn into the offscreen RT.
// Compiled with: glslc -x hlsl -fshader-stage=vert -fentry-point=main
//
// Uses a push constant (Time) to rotate the triangle about the origin.

struct PushConstants
{
    float Time;
    float _Pad0;
    float _Pad1;
    float _Pad2;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;

struct VSInput
{
    float3 Position : POSITION;
    float3 Color    : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Color    : COLOR;
};

VSOutput main(VSInput input)
{
    float  c  = cos(g_Push.Time);
    float  s  = sin(g_Push.Time);
    float2 p  = input.Position.xy;
    float2 rp = float2(c * p.x - s * p.y, s * p.x + c * p.y);

    VSOutput output;
    output.Position = float4(rp, input.Position.z, 1.0);
    output.Color    = input.Color;
    return output;
}
