#include "scene.h"

#ifdef USE_PBR
#define LIGHT_FUNCTION PBR
#else
#define LIGHT_FUNCTION BlinnPhong
#endif // USE_PBR

static const float PI = 3.141592;
static const float Epsilon = 0.00001;

struct MaterialInput
{
	float4 Albedo;
	float3 F0;
	float Roughness;
	float Metallic;
	float AO;
};

// Linear falloff.
float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// Shlick's approximation of the Fresnel factor.
float3 FresnelSchlick(float3 F0, float cosTheta)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 FresnelSchlickRoughness(float3 F0, float cosTheta, float roughness)
{
	float3 invRoughness = 1.0f - roughness;
	return F0 + (max(invRoughness, F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX / Towbridge - Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float NdfGGX(float cosLh, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float GaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float GaSchlickGGX(float cosLi, float cosLo, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return GaSchlickG1(cosLi, k) * GaSchlickG1(cosLo, k);
}

float3 BlinnPhong(float3 radiance, float3 toLight, float3 normal, float3 toEye, MaterialInput mat)
{
	const float m = (1.0f - mat.Roughness) * 256.0f;
	const float3 halfVec = normalize(toEye + toLight);

	const float cosToLight = max(dot(toLight, normal), 0.0f);
	const float cosHalfVec = max(0.0f, dot(normal, halfVec));

	const float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
	const float3 fresnelFactor = FresnelSchlick(mat.F0, cosHalfVec);

	const float3 specularFactor = fresnelFactor * roughnessFactor;

	return (mat.Albedo.rgb + specularFactor) * radiance * cosToLight;
}

float3 PBR(float3 radiance, float3 toLight, float3 normal, float3 toEye, MaterialInput mat)
{
	// cos between normal and toEye - TODO: Calculate this outside
	const float cosToEye = max(0.0, dot(normal, toEye));

	const float3 halfVec = normalize(toLight + toEye);

	const float cosToLight = max(0.0f, dot(normal, toLight));
	const float cosHalfVec = max(0.0f, dot(normal, halfVec));
	const float cosHalfVecToEye = max(0.0f, dot(halfVec, toEye));

	const float3 fresnelTerm = FresnelSchlick(mat.F0, cosHalfVecToEye);
	const float normalDistribution = NdfGGX(cosHalfVec, mat.Roughness);
	const float geometricAttenuation = GaSchlickGGX(cosToLight, cosToEye, mat.Roughness);

	// Lambert diffuse BRDF.
	const float3 diffuseFactor = lerp(float3(1.0f, 1.0f, 1.0f) - fresnelTerm, float3(0.0f, 0.0f, 0.0f), mat.Metallic);
	const float3 diffuse = diffuseFactor * mat.Albedo.rgb; // See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/ Should we scale to 1/PI

	// Cook-Torrance specular microfacet BRDF.
	const float3 specular = (fresnelTerm * normalDistribution * geometricAttenuation) / max(Epsilon, 4.0f * cosToLight * cosToEye);

	return (diffuse + specular) * radiance * cosToLight;
}

float3 ComputeDirectionalLight(Light light, MaterialInput mat, float3 normal, float3 toEye)
{
	const float3 radiance = light.Radiance;
	const float3 toLight = -light.Direction;
	return LIGHT_FUNCTION(radiance, toLight, normal, toEye, mat) * mat.AO;
}

float3 ComputePointLight(Light light, MaterialInput mat, float3 worldPos, float3 normal, float3 toEye)
{
	float3 toLight = light.Position - worldPos;
	const float distance = length(toLight);
	
	// Range test
	if (distance > light.Falloff.y) return float3(0.0f, 0.0f, 0.0f);

	// Normalize toLight
	toLight /= distance;

	const float3 radiance = light.Radiance * CalcAttenuation(distance, light.Falloff.x, light.Falloff.y);

	return LIGHT_FUNCTION(radiance, toLight, normal, toEye, mat);
}

float3 ComputeSpotLight(Light light, MaterialInput mat, float3 pos, float3 normal, float3 toEye)
{
	// TODO
	return float3(0.0f, 0.0f, 0.0f);
}

float3 ComputeAmbientLight(Light light, MaterialInput mat)
{
	return light.Radiance * mat.Albedo.rgb * mat.AO;
}

float3 ComputeIrradianceEffect(float3 irradiance, MaterialInput mat, float3 normal, float3 view)
{
	const float3 specularFactor = FresnelSchlickRoughness(mat.F0, max(dot(normal, view), 0.0), mat.Roughness);
	const float3 diffuseFactor = 1.0f - specularFactor;
	const float3 diffuse = irradiance * mat.Albedo.rgb;
	const float ao = mat.AO;
	return diffuseFactor * diffuse * ao;
}