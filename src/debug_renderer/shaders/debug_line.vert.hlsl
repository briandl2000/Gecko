struct DebugLine
{
    float2 A;
    float2 B;
    float3 Color;
    float Thickness;
};

struct PushConstants
{
    float2 ViewportPx;
    float2 Padding;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;
[[vk::binding(0)]] StructuredBuffer<DebugLine> g_Lines : register(t0);

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Color : COLOR;
};

static const uint CornerLut[6] = {0u, 1u, 2u, 2u, 1u, 3u};

VertexOutput main(uint vertexId : SV_VertexID)
{
    uint lineIndex = vertexId / 6u;
    uint corner = CornerLut[vertexId % 6u];
    DebugLine value = g_Lines[lineIndex];

    float2 direction = value.B - value.A;
    float lengthPx = max(length(direction), 0.00001f);
    float2 perpendicular = float2(-direction.y, direction.x) / lengthPx;
    float2 base = corner >= 2u ? value.B : value.A;
    float side = (corner == 1u || corner == 3u) ? 1.0f : -1.0f;
    float2 positionPx = base + perpendicular * side * value.Thickness * 0.5f;

    float2 ndc = float2(
        positionPx.x / g_Push.ViewportPx.x * 2.0f - 1.0f,
        1.0f - positionPx.y / g_Push.ViewportPx.y * 2.0f
    );

    VertexOutput output;
    output.Position = float4(ndc, 0.0f, 1.0f);
    output.Color = value.Color;
    return output;
}
