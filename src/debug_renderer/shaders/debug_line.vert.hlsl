
// Thick screenspace line vertex shader.
// One DebugLine struct in the structured buffer is expanded to 6 vertices
// (a screen-aligned quad, two triangles) using SV_VertexID.

struct DebugLine
{
    float2 A;          // start, pixels, top-left origin
    float2 B;          // end,   pixels
    float3 Color;      // RGB, linear
    float  Thickness;  // pixels
};

struct PushConstants
{
    float2 ViewportPx;
    float2 _Pad;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;
[[vk::binding(0)]] StructuredBuffer<DebugLine> g_Lines : register(t0);

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Color    : COLOR;
};

// Triangulation: corners 0,1,2 form one triangle; 2,1,3 the other.
// Quad corner indices: 0 = A-perp, 1 = A+perp, 2 = B-perp, 3 = B+perp.
static const uint CornerLut[6] = { 0u, 1u, 2u, 2u, 1u, 3u };

VSOutput main(uint vid : SV_VertexID)
{
    uint lineIdx   = vid / 6u;
    uint cornerIdx = vid % 6u;
    uint c         = CornerLut[cornerIdx];

    DebugLine ln = g_Lines[lineIdx];

    bool atB    = (c >= 2u);
    bool atPlus = (c == 1u) || (c == 3u);

    float2 d    = ln.B - ln.A;
    float  L    = max(length(d), 1e-5);
    float2 dir  = d / L;
    float2 perp = float2(-dir.y, dir.x);
    float  halfT = ln.Thickness * 0.5f;

    float2 base   = atB ? ln.B : ln.A;
    float2 offset = perp * (atPlus ? halfT : -halfT);
    float2 posPx  = base + offset;

    float2 ndc = float2(
        (posPx.x / g_Push.ViewportPx.x) * 2.0f - 1.0f,
        1.0f - (posPx.y / g_Push.ViewportPx.y) * 2.0f
    );

    VSOutput o;
    o.Position = float4(ndc, 0.0f, 1.0f);
    o.Color    = ln.Color;
    return o;
}
