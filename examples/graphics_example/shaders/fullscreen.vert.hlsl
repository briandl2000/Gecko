// Fullscreen-triangle vertex shader. Generates UVs from SV_VertexID so no
// vertex buffer is required; draw three vertices.
// Compiled with: glslc -x hlsl -fshader-stage=vert -fentry-point=main

struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    output.UV       = float2((vertexId << 1) & 2, vertexId & 2);
    output.Position = float4(output.UV * 2.0 - 1.0, 0.0, 1.0);
    return output;
}
