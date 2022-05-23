#include "samplers.h"
#include "scene.h"
#include "lighting.h"
#include "light_culling.h"
#include "util.h"

struct VertexInput
{
	float3 Position : SV_POSITION;
	float2 UV : TEXCOORD;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
	uint DrawableIndex : DRAWABLE_INDEX;
};

struct VertexOut
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : WORLD_POS;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD;
	nointerpolation uint MaterialIndex : MAT_INDEX;
};

cbuffer CameraCB : register(b0)
{
	Camera CamData;
}

cbuffer SceneInfoCB : register(b1)
{
	SceneInfo SceneInfoData;
}

Texture2DArray Textures : register(t0);
StructuredBuffer<Light> Lights : register(t3);
StructuredBuffer<Entity> Entities : register(t5);
StructuredBuffer<Material> Materials : register(t6);
StructuredBuffer<Drawable> Drawables : register(t7);
StructuredBuffer<uint> VisibleLights : register(t8);

VertexOut VS(VertexInput IN)
{
	Drawable d = Drawables[IN.DrawableIndex];
	const uint entityIndex = d.EntityIndex;
	const float4x4 modelToWorld = Entities[entityIndex].ModelToWorld;

	VertexOut OUT;
	OUT.Position = GetClipPosWithJitter(IN.Position, modelToWorld, CamData);
	OUT.WorldPosition = mul(float4(IN.Position, 1.0), modelToWorld);
	OUT.Normal = mul(IN.Normal, (float3x3) modelToWorld); // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	OUT.UV = IN.UV;
	OUT.MaterialIndex = d.MaterialIndex;
	return OUT;
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

	const float3 normal = normalize(IN.Normal);
	const float3 view = normalize(CamData.Position - IN.WorldPosition);

	float4 litColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

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
			litColor.rgb += ComputeDirectionalLight(l, mat, normal, view);
			break;
		case LIGHT_TYPE_POINT:
			litColor.rgb += ComputePointLight(l, mat, IN.WorldPosition, normal, view);
			break;
		case LIGHT_TYPE_SPOT:
			litColor.rgb += ComputeSpotLight(l, mat, IN.WorldPosition, normal, view);
			break;
		case LIGHT_TYPE_AMBIENT:
			litColor.rgb += ComputeAmbientLight(l, mat);
			break;
		}
	}

#ifdef ALPHA_BLEND
	litColor.a = mat.Albedo.a;
#else
	litColor.a = 1.0f;
#endif // ALPHA_BLEND

	return litColor;
}