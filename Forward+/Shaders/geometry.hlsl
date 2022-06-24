#include "samplers.h"
#include "scene.h"
#include "lighting.h"
#include "light_culling.h"
#include "util.h"
#include "pipeline.h"

struct VertexOut
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : WORLD_POS;
	float3 Normal : NORMAL;
	float3 Tangent : TANGENT;
	float3 Bitangent : BITANGENT;
	float2 UV : TEXCOORD;
	nointerpolation uint MaterialIndex : MAT_INDEX;
};

cbuffer Constants : register(b0)
{
	Camera MainCamera;
	SceneInfo SceneInfoData;
}

StructuredBuffer<Light> Lights : register(t0);
StructuredBuffer<uint> VisibleLights : register(t1);
Texture2D<float> Shadowmask : register(t2);
TextureCube IrradianceMap : register(t3);

VertexOut VS(VertexPipelineInput IN)
{
    const Drawable d = Drawables[IN.DrawableInstance];
    const Vertex vert = GetWorldSpaceVertex(IN);
	
	VertexOut OUT;
    OUT.Position = GetClipPosWithJitter(vert.Position, MainCamera);
    OUT.WorldPosition = vert.Position;
    OUT.Normal = vert.Normal;
    OUT.Tangent = vert.Tangent.xyz; // TODO: What should I do with tangent.w ?
    OUT.Bitangent = normalize(cross(vert.Normal, vert.Tangent.xyz));
    OUT.UV = vert.Texcoord;
	OUT.MaterialIndex = d.MaterialIndex;
	return OUT;
}

float3 ApplyFog(float3 color, float depth)
{
	const float3 FogColor = float3(0.6f, 0.67f, 0.73f);
	const float FogStart = 0.99f;
	const float FogEnd = 1.0f;

	const float distance = 1.0f - depth;
	const float fogFactor = smoothstep(FogStart, FogEnd, distance);

	return lerp(color, FogColor, fogFactor);
}

float4 PS(VertexOut IN) : SV_TARGET
{
	const Material matParams = Materials[IN.MaterialIndex];
	const float4 albedo = Textures.Sample(s_AnisoWrap, float3(IN.UV, matParams.Albedo) );

#ifdef ALPHA_DISCARD
	if (albedo.a < 0.05f)
		clip(-1.0f);
#endif // ALPHA_DISCARD

	float2 metallicRoughness = Textures.Sample(s_LinearWrap, float3(IN.UV, matParams.MetallicRoughness)).rg;

	MaterialInput mat;
	mat.Albedo = albedo;
	mat.Albedo.rgb *= matParams.AlbedoFactor;
	mat.Metallic = metallicRoughness.r * matParams.MetallicFactor;
	mat.Metallic = min(0.99f, mat.Metallic);
	mat.Roughness = metallicRoughness.g * matParams.RoughnessFactor;
	mat.Roughness = min(0.99f, mat.Roughness);
	mat.F0 = lerp(matParams.FresnelR0, albedo.rgb, mat.Metallic);

	const float3x3 TBN = float3x3(normalize(IN.Tangent), normalize(IN.Bitangent), normalize(IN.Normal));
	const float3 normalValue = 2.0f * Textures.Sample(s_LinearWrap, float3(IN.UV, matParams.Normal)).rgb - 1.0f;

	const float3 normal = normalize(mul(normalValue, TBN));
	const float3 view = normalize(MainCamera.Position - IN.WorldPosition);

	float4 litColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	const float2 shadowMaskUV = IN.Position.xy / SceneInfoData.ScreenSize;
	const float shadowFactor = Shadowmask.Sample(s_LinearWrap, shadowMaskUV);

#ifdef DISABLE_LIGHT_CULLING
	for(uint i=0; i < SceneInfoData.NumLights; i++)
	{
		const Light l = Lights[i];
#else
	const uint2 tileIndex = GetTileIndexFromPosition(IN.Position.xyz);
	const uint visibleLightOffset = GetOffsetFromTileIndex(SceneInfoData, tileIndex);
	for (uint i = visibleLightOffset; VisibleLights[i] != VISIBLE_LIGHT_END; i++)
	{
		const uint lightIndex = VisibleLights[i];
		const Light l = Lights[lightIndex];
#endif // DISABLE_LIGHT_CULLING

		switch (l.Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			litColor.rgb += shadowFactor * ComputeDirectionalLight(l, mat, normal, view);
			break;
		case LIGHT_TYPE_POINT:
			litColor.rgb += ComputePointLight(l, mat, IN.WorldPosition, normal, view);
			break;
		case LIGHT_TYPE_SPOT:
			litColor.rgb += ComputeSpotLight(l, mat, IN.WorldPosition, normal, view);
			break;

		// We are using ambient lights only when IBL is not turned on
#ifndef USE_IBL
		case LIGHT_TYPE_AMBIENT:
			litColor.rgb += ComputeAmbientLight(l, mat);
			break;
#endif // !USE_IBL
		}
	}

#ifdef USE_IBL
	const float3 irradiance = IrradianceMap.Sample(s_LinearWrap, normal).rgb;
	litColor.rgb += ComputeIrradianceEffect(irradiance, mat, normal, view);
#endif // USE_IBL

	litColor.rgb = ApplyFog(litColor.rgb, IN.Position.z / IN.Position.w);

#ifdef ALPHA_BLEND
	litColor.a = mat.Albedo.a;
#else
	litColor.a = 1.0f;
#endif // ALPHA_BLEND

	return litColor;
}