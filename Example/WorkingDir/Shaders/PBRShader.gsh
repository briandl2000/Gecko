#include "ShaderDefines.h"

RW_Texture2D_Arr<float4> Output : register(u0);

RW_Texture2D_Arr<float4> AlbedoTexture		: register(u1);
RW_Texture2D_Arr<float4> NormalTexture		: register(u2);
RW_Texture2D_Arr<float4> PositionTexture	: register(u3);
RW_Texture2D_Arr<float4> EmissiveTexture	: register(u4);
RW_Texture2D_Arr<float4> MROTexture			: register(u5);
Texture2D<float4> specularBRDF_LUT			: register(t0);

TextureCube specularTexture : register(t1);
TextureCube irradianceTexture : register(t2);
Texture2D<float> ShadowMap			: register(t3);


SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

ConstantBuffer<SceneData> sd : register(b0);

struct PBRData
{
	uint width;
	uint height;
};

DYNAMIC_CALL_DATA(PBRData, pbrData, b1);

// ----------------------------------------------------------------------------
static const float PI = 3.141592;
static const float Epsilon = 0.00001;
float ndfGGX(float cosLh, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) *gaSchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
float3 fresnelSchlick(float3 F0, float cosTheta)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

uint querySpecularTextureLevels()
{
	uint width, height, levels;
	specularTexture.GetDimensions(0, width, height, levels);
	return levels;
}

// ---------------------------------------


float pcf(float2 projCoords, float currentDepth, float bias)
{
	int width;
	int height;
	ShadowMap.GetDimensions(width, height);

	float shadow = 0.0;
	float2 texelSize = 1.0 / float2(width, height);
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = SAMPLE_TEXTURE_LEVEL(ShadowMap, LinearSampler, projCoords + float2(x, y) * texelSize, 0.);
			shadow += currentDepth - bias < pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9.0;
	return shadow;
}

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	float3 normal = NormalTexture[DTid].xyz;

	float3 emmisive = EmissiveTexture[DTid].xyz;

	if (length(normal) < 0.01)
	{
		Output[DTid] = float4(emmisive, 1.);
		return;
	}
	normal = normalize(normal);


	float3 albedo = AlbedoTexture[DTid].xyz;
	float3 worldPos = PositionTexture[DTid].xyz;

	float metalness = MROTexture[DTid].x;
	float roughness = MROTexture[DTid].y;
	float ao = MROTexture[DTid].z;

	float3 lightDir = sd.LightDirection;
	lightDir = normalize(lightDir);

	float3 Lo = normalize(sd.camPos - worldPos);

	// Get current fragment's normal and transform to world space.

	// Angle between surface normal and outgoing light direction.
	float cosLo = max(0.0, dot(normal, Lo));

	// Specular reflection vector.
	float3 Lr = 2.0 * cosLo * normal - Lo;

	float3 F0 = float3(0.04, 0.04, 0.04);
	F0 = lerp(F0, albedo.xyz, metalness);



	float3 directLighting = 0.0;
	{
		float3 Li = normalize(-lightDir);
		float3 Lradiance = sd.LightColor * sd.LighIntensity;

		// Half-vector between Li and Lo.
		float3 Lh = normalize(Li + Lo);

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0, dot(normal, Li));
		float cosLh = max(0.0, dot(normal, Lh));

		// Calculate Fresnel term for direct lighting. 
		float3 F = fresnelSchlick(F0, max(0.0, dot(Lh, Lo)));
		// Calculate normal distribution for specular BRDF.
		float D = min(ndfGGX(cosLh, roughness), sd.LighIntensity);
		// Calculate geometric attenuation for specular BRDF.
		float G = gaSchlickGGX(cosLi, cosLo, roughness);

		// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
		// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
		// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
		float3 kd = lerp(float3(1., 1., 1.) - F, float3(0., 0., 0.), metalness);

		// Lambert diffuse BRDF.
		// We don't scale by 1/PI for lighting & material units to be more convenient.
		// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
		float3 diffuseBRDF = kd * albedo;

		// Cook-Torrance specular microfacet BRDF.
		float3 specularTerm = (D * G) / max(Epsilon, 4.0 * cosLi * cosLo);
		//specularTerm = sqrt(max(0.0001, specularTerm));
		float3 specularBRDF = F * specularTerm;
		// Total contribution for this light.
		//directLighting += specularBRDF;// specularBRDF;//albedo * Lradiance * cosLi;
		directLighting += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
	}

	if (sd.useRaytracing)
	{
		
		directLighting *= SAMPLE_TEXTURE_LEVEL(ShadowMap, PointSampler, (DTid.xy+.5)/float2(1920., 1080.), 0.);
	}
	else
	{
		float4 ShadowProjection = mul(sd.ShadowMapProjection, float4(worldPos, 1.));

	
		ShadowProjection /= ShadowProjection.w;
		ShadowProjection.xy = ShadowProjection.xy *0.5 + 0.5;
		ShadowProjection.y = 1.-ShadowProjection.y;
		if (ShadowProjection.x > 0. && ShadowProjection.x < 1. &&
			ShadowProjection.y > 0. && ShadowProjection.y < 1. &&
			ShadowProjection.z > 0. && ShadowProjection.z < 1.)
			{
			directLighting *= pcf(ShadowProjection.xy, ShadowProjection.z, 4.*1e-4);
		}
	}
	// IBL Here

	float3 ambientLighting;
	{
		// Sample diffuse irradiance at normal direction.
		float3 irradiance = SAMPLE_TEXTURE_LEVEL(irradianceTexture, LinearSampler, normal, 0.).rgb;

		// Calculate Fresnel term for ambient lighting.
		// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
		// use cosLo instead of angle with light's half-vector (cosLh above).
		// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
		float3 F = fresnelSchlick(F0, cosLo);

		// Get diffuse contribution factor (as with direct lighting).
		float3 kd = lerp(1 - F, 0.0, metalness);

		// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
		float3 diffuseIBL = kd * albedo * irradiance;

		// Sample pre-filtered specular reflection environment at correct mipmap level.
		uint specularTextureLevels = querySpecularTextureLevels();
		float3 specularIrradiance = SAMPLE_TEXTURE_LEVEL(specularTexture, LinearSampler, Lr, roughness * specularTextureLevels).rgb;
		//specularTexture.SampleLOD

		// Split-sum approximation factors for Cook-Torrance specular BRDF.
		float2 specularBRDF = SAMPLE_TEXTURE_LEVEL(specularBRDF_LUT, PointSampler, float2(cosLo, 1.-roughness), 0.).rg;

		// Total specular IBL contribution.
		float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;

		// Total ambient lighting contribution.
		ambientLighting = (diffuseIBL + specularIBL) * sd.ambientIntensity;
	}

	float3 color = directLighting + ambientLighting * ao + emmisive;
	// HDR tonemapping

	//color = color / (color + float3(1.0, 1.0, 1.0));
	//color = float3(1.0, 1.0, 1.0) - exp(-color * sd.exposure);
	//color = pow(color, float3(1. / 2.2, 1. / 2.2, 1. / 2.2));
	Output[DTid] = float4(color, 1.);

}
#endif