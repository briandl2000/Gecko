
// Thick screenspace line vertex shader.
// One DebugLine struct in the structured buffer is expanded to 6 vertices
// (a screen-aligned quad, two triangles) using SV_VertexID.

struct DebugChar
{
    float2 Position;
    float Size;
    float _Pad;
    float3 Color;
    float _Pad2;
};

struct PushConstants
{
    float2 ViewportPx;
    float2 _Pad;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;
[[vk::binding(0)]] StructuredBuffer<DebugChar> g_Chars : register(t0);

struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV    : TEX_COORD;
};

static const float2 VertexPositions[6] = {
    float2(-0.5, -0.5),
    float2( 0.5, -0.5),
    float2(-0.5,  0.5),

    float2( 0.5, -0.5),
    float2( 0.5,  0.5),
    float2(-0.5,  0.5)
};

static const float2 VertexUVs[6] = {
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(0.0, 1.0),

    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0)
};

VSOutput main(uint vid : SV_VertexID)
{
    uint charIdx   = vid / 6u;
    uint cornerIdx = vid % 6u;
    float2 vertPos = VertexPositions[cornerIdx];

    DebugChar ch = g_Chars[charIdx];

    float2 posPx = vertPos * ch.Size + ch.Position;

    float2 ndc = float2(
        (posPx.x / g_Push.ViewportPx.x) * 2.0f - 1.0f,
        1.0f - (posPx.y / g_Push.ViewportPx.y) * 2.0f
    );

    VSOutput o;
    o.Position = float4(ndc, 0.0f, 1.0f);
    o.UV    = VertexUVs[cornerIdx];
    return o;
}
