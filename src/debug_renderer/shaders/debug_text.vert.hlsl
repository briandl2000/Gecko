
// Thick screenspace line vertex shader.
// One DebugLine struct in the structured buffer is expanded to 6 vertices
// (a screen-aligned quad, two triangles) using SV_VertexID.

struct DebugChar
{
    float2 Position;
    float Size;
    uint GlyphIndex;
    float3 Color;
    float _Pad2;
};

struct PushConstants
{
    float2 ViewportPx;
    float2 _Pad;
};

// generic data about the glyph atlas
struct GlyphData
{
  uint GlyphWidth; // width in pixels of the glyphs
  uint GlyphHeight; // height in pixels of the glyphs
  uint NumberOfGlyphsPerRow; // number of glyphs per row in the atlas
  uint NumberOfGlyphsPerColumn; // number of glyphs per column in the atlas
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;
[[vk::binding(0)]] StructuredBuffer<DebugChar> g_Chars : register(t0);
[[vk::binding(1)]] ConstantBuffer<GlyphData> g_GlyphData : register(t1);

struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV    : TEX_COORD;
    float3 Color : COLOR;
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

    uint glyphIndex = ch.GlyphIndex;

    uint glyphX = glyphIndex % g_GlyphData.NumberOfGlyphsPerRow;
    uint glyphY = glyphIndex / g_GlyphData.NumberOfGlyphsPerRow;

    float2 atlasPixelSize = float2(
        g_GlyphData.GlyphWidth  * g_GlyphData.NumberOfGlyphsPerRow,
        g_GlyphData.GlyphHeight * g_GlyphData.NumberOfGlyphsPerColumn
    );

    float2 glyphPixelMin = float2(
        glyphX * g_GlyphData.GlyphWidth,
        glyphY * g_GlyphData.GlyphHeight
    );

    float2 glyphPixelSize = float2(
        g_GlyphData.GlyphWidth,
        g_GlyphData.GlyphHeight
    );

    float2 uv = (glyphPixelMin + VertexUVs[cornerIdx] * glyphPixelSize) / atlasPixelSize;


    VSOutput o;
    o.Position = float4(ndc, 0.0f, 1.0f);
    o.UV    = uv;
    o.Color = ch.Color;
    return o;
}
