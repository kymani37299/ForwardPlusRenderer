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
	float4 ClipPosition : CLIP_POS;
	float4 LastFramePosition : LAST_FRAME_POS;
	float3 WorldPosition : WORLD_POS;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD;
	nointerpolation uint MaterialIndex : MAT_INDEX;
};

struct PixelOut
{
	float4 Albedo : SV_TARGET0;
	float2 MotionVector : SV_TARGET1;
};

cbuffer CameraCB : register(b0)
{
	Camera CamData;
}

#ifdef DISABLE_LIGHT_CULLING
cbuffer SceneInfoCB : register(b1)
{
	SceneInfo SceneInfoData;
}
#else
cbuffer TiledCullingInfoCB : register(b1)
{
	TiledCullingInfo TiledCullingInfoData;
}
#endif // DISABLE_LIGHT_CULLING

cbuffer CameraCBLastFrame : register(b2)
{
	Camera CamDataLastFrame;
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

	// TODO: lastFrameModelToWorld - to use in last frame position
	const float4x4 modelToWorld = Entities[entityIndex].ModelToWorld;

	VertexOut OUT;
	OUT.Position = GetClipPosWithJitter(IN.Position, modelToWorld, CamData);
	OUT.ClipPosition = OUT.Position;
	OUT.LastFramePosition = GetClipPos(IN.Position, modelToWorld, CamDataLastFrame);
	OUT.WorldPosition = mul(float4(IN.Position, 1.0), modelToWorld);
	OUT.Normal = mul(IN.Normal, (float3x3) modelToWorld); // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
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

PixelOut PS(VertexOut IN)
{
	const Material matParams = Materials[IN.MaterialIndex];
	const float4 albedo = Textures.Sample(s_AnisoWrap, float3(IN.UV, matParams.Albedo) );

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

	const float3 normal = normalize(IN.Normal);
	const float3 view = normalize(CamData.Position - IN.WorldPosition);

	float4 litColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

#ifdef DISABLE_LIGHT_CULLING
	for(uint i=0; i < SceneInfoData.NumLights; i++)
	{
		const Light l = Lights[i];
#else
	const uint2 tileIndex = GetTileIndexFromPosition(IN.Position.xyz);
	const uint visibleLightOffset = GetOffsetFromTileIndex(TiledCullingInfoData, tileIndex);
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

	PixelOut OUT;
	OUT.Albedo = litColor;
	OUT.MotionVector = CalculateMotionVector(IN.ClipPosition, IN.LastFramePosition, float2(1024, 768)); // TODO: Use real screen size
	return OUT;
}