
#include "ShaderDefines.h"

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float> RenderTarget : register(u0);
RWTexture2D<float4> Positions : register(u1);
RWTexture2D<float4> Normals : register(u2);
ConstantBuffer<SceneData> sd : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void MyRaygenShader()
{

    float2 uv = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

    float3 rayDir = -sd.LightDirection;
    float3 origin = Positions[DispatchRaysIndex().xy].xyz;
    float3 normal = Normals[DispatchRaysIndex().xy].xyz;

    // Trace the ray.
    // Set the ray's extents.
    RayDesc ray;
    ray.Origin = origin+normal*0.01;
    ray.Direction = rayDir;
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    
    RayPayload payload = { float4(1., 1., 1., 1.) };
    TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);

    // Write the raytraced color to the output texture.
    RenderTarget[DispatchRaysIndex().xy] = payload.color.r;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    payload.color = float4(0.0, 0.0, 0.0, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(1., 1., 1., 1);
}

