#include "samplers.h"
#include "scene.h"
#include "lighting.h"
#include "light_culling.h"

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

cbuffer TiledCullingInfoCB : register(b1)
{
	TiledCullingInfo TiledCullingInfoData;
}

cbuffer LightSpaceCB : register(b3)
{
	float4x4 WorldToLightClip;
}

Texture2DArray Textures : register(t0);
StructuredBuffer<Light> Lights : register(t3);
Texture2D Shadowmap : register(t4);
StructuredBuffer<Entity> Entities : register(t5);
StructuredBuffer<Material> Materials : register(t6);
StructuredBuffer<Drawable> Drawables : register(t7);
StructuredBuffer<uint> VisibleLights : register(t8);

VertexOut VS(VertexInput IN)
{
	Drawable d = Drawables[IN.DrawableIndex];
	uint entityIndex = d.EntityIndex;
	const float4 modelPos = float4(IN.Position, 1.0f);
	const float4 worldPos = mul(modelPos, Entities[entityIndex].ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	VertexOut OUT;
	OUT.Position = clipPos;
	OUT.WorldPosition = worldPos.xyz;
	OUT.Normal = mul(IN.Normal, (float3x3) Entities[entityIndex].ModelToWorld); // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	OUT.UV = IN.UV;
	OUT.MaterialIndex = d.MaterialIndex;
	return OUT;
}

bool IsInShadow(float3 worldPos)
{
	float4 lightClip = mul(float4(worldPos, 1.0f), WorldToLightClip);
	float3 lightNDC = lightClip.xyz / lightClip.w;

	float3 lightUV = lightNDC * 0.5f + 0.5f;
	if (lightUV.x <= 0.0f || lightUV.x >= 1.0f || lightUV.y <= 0.0f || lightUV.y >= 1.0f)
	{
		return false;
	}

	return lightNDC.z < Shadowmap.Sample(s_LinearWrap, lightUV.xy).r;
}

float4 PS(VertexOut IN) : SV_Target
{
	Material matParams = Materials[IN.MaterialIndex];
	float4 albedo = Textures.Sample(s_AnisoWrap, float3(IN.UV, matParams.Albedo) );

#ifdef ALPHA_DISCARD
	if (albedo.a < 0.05f)
		clip(-1.0f);
#endif // ALPHA_DISCARD

	//if (IsInShadow(IN.WorldPosition))
	//{
	//	return float4(0.0f, 0.0f, 0.0f, 1.0f);
	//}

	MaterialInput mat;
	mat.Albedo = albedo;
	mat.Albedo.rgb *= matParams.AlbedoFactor;
	mat.FresnelR0 = matParams.FresnelR0;
	mat.Roughness = Textures.Sample(s_LinearWrap, float3(IN.UV, matParams.MetallicRoughness)).g * matParams.RoughnessFactor;
	mat.Roughness = min(0.99f, mat.Roughness);

	float3 normal = normalize(IN.Normal);
	float3 view = normalize(CamData.Position - IN.WorldPosition);

	float4 litColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	uint2 tileIndex = GetTileIndexFromPosition(IN.Position.xyz);
	uint visibleLightOffset = GetOffsetFromTileIndex(TiledCullingInfoData, tileIndex);

	for (uint i = visibleLightOffset; VisibleLights[i] != VISIBLE_LIGHT_END; i++)
	{
		uint lightIndex = VisibleLights[i];
		Light l = Lights[lightIndex];
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