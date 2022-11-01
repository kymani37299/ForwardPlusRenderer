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
Texture2D<float> AmbientOcclusion : register(t4);

VertexOut VS(VertexPipelineInput IN)
{
    const Drawable d = GetDrawable();
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

float3 ApplyFog(float3 color, float depth, float3 fogColor)
{
	const float FogStart = 0.99f;
	const float FogEnd = 1.0f;

	const float distance = 1.0f - depth;
	const float fogFactor = smoothstep(FogStart, FogEnd, distance);

	return lerp(color, fogColor, fogFactor);
}

float4 PS(VertexOut IN) : SV_TARGET
{
	const Material matParams = Materials[IN.MaterialIndex];
	const float4 albedo = Textures[matParams.Albedo].Sample(s_AnisoWrap, IN.UV);

#ifdef ALPHA_DISCARD
	if (albedo.a < 0.05f)
		clip(-1.0f);
#endif // ALPHA_DISCARD

	const float2 screenUV = IN.Position.xy / SceneInfoData.ScreenSize;

	float2 metallicRoughness = Textures[matParams.MetallicRoughness].Sample(s_LinearWrap, IN.UV).rg;

	MaterialInput mat;
	mat.Albedo = albedo;
	mat.Albedo.rgb *= matParams.AlbedoFactor;
	mat.Metallic = metallicRoughness.r * matParams.MetallicFactor;
	mat.Metallic = min(0.99f, mat.Metallic);
	mat.Roughness = metallicRoughness.g * matParams.RoughnessFactor;
	mat.Roughness = min(0.99f, mat.Roughness);
	mat.F0 = lerp(matParams.FresnelR0, albedo.rgb, mat.Metallic);
	mat.AO = AmbientOcclusion.Sample(s_LinearWrap, screenUV);

	const float3x3 TBN = float3x3(normalize(IN.Tangent), normalize(IN.Bitangent), normalize(IN.Normal));
	const float3 normalValue = 2.0f * Textures[matParams.Normal].Sample(s_LinearWrap, IN.UV).rgb - 1.0f;

	const float3 normal = normalize(mul(normalValue, TBN));
	const float3 view = normalize(MainCamera.Position - IN.WorldPosition);

	float4 litColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	
#ifdef DISABLE_LIGHT_CULLING
	[loop]
	for (uint i = 0; i < SceneInfoData.NumLights; i++)
	{
		litColor.rgb += ComputeLightEffect(Lights[i], mat, IN.WorldPosition, normal, view);
	}
#else
	const uint2 tileIndex = GetTileIndexFromPosition(IN.Position.xyz);
	const uint visibleLightOffset = GetOffsetFromTileIndex(SceneInfoData, tileIndex);

	[loop]
	for (uint i = visibleLightOffset; VisibleLights[i] != VISIBLE_LIGHT_END; i++)
	{
		litColor.rgb += ComputeLightEffect(Lights[VisibleLights[i]], mat, IN.WorldPosition, normal, view);
	}
#endif // DISABLE_LIGHT_CULLING

#ifdef USE_IBL
	const float3 fogColor = IrradianceMap.Sample(s_LinearWrap, SceneInfoData.DirLight.Direction).rgb;
	const float3 ambientIrradiance = IrradianceMap.Sample(s_LinearWrap, normal).rgb;
#else
	const float3 fogColor = float3(0.6f, 0.67f, 0.73f);
	const float3 ambientIrradiance = SceneInfoData.AmbientRadiance;
#endif // USE_IBL

	// Directional
	const float2 shadowMaskUV = screenUV;
	const float shadowFactor = Shadowmask.Sample(s_LinearWrap, shadowMaskUV);
	litColor.rgb += shadowFactor * ComputeDirectionalEffect(SceneInfoData.DirLight, mat, normal, view);

	// Ambient
	litColor.rgb += ComputeAmbientEffect(ambientIrradiance, mat, normal, view);

	// Distance fog
	litColor.rgb = ApplyFog(litColor.rgb, IN.Position.z / IN.Position.w, fogColor);

#ifdef ALPHA_BLEND
	litColor.a = mat.Albedo.a;
#else
	litColor.a = 1.0f;
#endif // ALPHA_BLEND

	return litColor;
}