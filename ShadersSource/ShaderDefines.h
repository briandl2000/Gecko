#if defined(HLSL)
#define S_OUTPUT_POSITION SV_POSITION
#define S_INPUT_VERTEX_ID SV_VertexID
#define CBuffer cbuffer
#define S_OUTPUT_POSITION SV_POSITION
#define S_OUTPUT_TARGET SV_TARGET
#define S_OUTPUT_TARGET0 SV_TARGET0
#define S_OUTPUT_TARGET1 SV_TARGET1
#define S_OUTPUT_TARGET2 SV_TARGET2
#define S_OUTPUT_TARGET3 SV_TARGET3
#define S_OUTPUT_TARGET4 SV_TARGET4
#define S_OUTPUT_TARGET5 SV_TARGET5
#define S_OUTPUT_TARGET6 SV_TARGET6
#define S_OUTPUT_TARGET7 SV_TARGET7
#define S_OUTPUT_DEPTH SV_Depth
#define Texture2D_Arr Texture2DArray
#define RW_Texture2D_Arr RWTexture2DArray
#define SAMPLE_TEXTURE_LEVEL(texture, sampler, uv, level) texture.SampleLevel(sampler, uv, level)
#define SAMPLE_TEXTURE2D_LEVEL(texture, sampler, uv, level) texture.SampleLevel(sampler, float3(uv, 0.), level)
#define N_THREADS(X,Y,Z) numthreads(X, Y, Z)
#define S_INPUT_DISPATCH_ID SV_DispatchThreadID
#else
struct float3 {};
struct float4 {
    float4(float3, float) {};
};
struct float2 {};
struct float4x4 {};
template<typename T>
struct ConstantBuffer {
    T& operator .() { return t; }
    T t;
};
float4x4 mul(float4x4, float4x4) {};
float4 mul(float4x4, float4) {};
float4 mul(float4, float4x4) {};
#endif

#if defined(HLSL)
#define DYNAMIC_CALL_DATA(bufferType, bufferName, bufferLocation) ConstantBuffer<bufferType> bufferName : register(bufferLocation);
#endif

struct SceneData
{
    float4x4 view;
    float4x4 invView;
    float4x4 projection;
    float4x4 invProjection;
    float4x4 viewOrientation;
    float4x4 invViewOrientation;
    float4x4 ShadowMapProjection;
    float4x4 ShadowMapProjectionInv;
	float3 camPos;
	float LighIntensity;
	float3 LightDirection;
	float exposure;
	float3 LightColor;
    float ambientIntensity;
    float useRaytracing;
};


float4 DownsampleBox13Tap(Texture2D<float4> tex, SamplerState samplerTex, float2 uv, float2 texelSize)
{
    float4 A = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2(-1.0, -1.0), 0.f);
    float4 B = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2( 0.0, -1.0), 0.f);
    float4 C = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2( 1.0, -1.0), 0.f);
    float4 D = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2(-0.5, -0.5), 0.f);
    float4 E = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2( 0.5, -0.5), 0.f);
    float4 F = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2(-1.0,  0.0), 0.f);
    float4 G = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv                                 , 0.f);
    float4 H = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2( 1.0,  0.0), 0.f);
    float4 I = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2(-0.5,  0.5), 0.f);
    float4 J = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2( 0.5,  0.5), 0.f);
    float4 K = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2(-1.0,  1.0), 0.f);
    float4 L = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2( 0.0,  1.0), 0.f);
    float4 M = SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + texelSize * float2( 1.0,  1.0), 0.f);

    float2 div = (1.0 / 4.0) * float2(0.5, 0.125);

    float4 o = (D + E + I + J) * div.x;
    o += (A + B + G + F) * div.y;
    o += (B + C + H + G) * div.y;
    o += (F + G + L + K) * div.y;
    o += (G + H + M + L) * div.y;

    return o;
}

float4 UpsampleTent(Texture2D<float4> tex, SamplerState samplerTex, float2 uv, float2 texelSize, float4 sampleScale)
{
    float4 d = texelSize.xyxy * float4(1.0, 1.0, -1.0, 0.0) * sampleScale;

    float4 s;
    s =  SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv - d.xy, 0);
    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv - d.wy, 0) * 2.0;
    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv - d.zy, 0);

    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + d.zw, 0) * 2.0;
    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv       , 0) * 4.0;
    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + d.xw, 0) * 2.0;

    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + d.zy, 0);
    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + d.wy, 0) * 2.0;
    s += SAMPLE_TEXTURE_LEVEL(tex, samplerTex, uv + d.xy, 0);

    return s * (1.0 / 16.0);
}