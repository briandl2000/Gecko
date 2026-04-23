// Vertex shader for the coloured triangle drawn into the offscreen RT.
// Compiled with: glslc -x hlsl -fshader-stage=vert -fentry-point=main

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
    VSOutput output;
    output.Position = float4(input.Position, 1.0);
    output.Color    = input.Color;
    return output;
}
