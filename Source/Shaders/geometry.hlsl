#include "samplers.h"
#include "scene.h"
#include "lighting.h"

struct VertexInput
{
	float3 Position : SV_POSITION;
	float2 UV : TEXCOORD;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
};

struct VertexOut
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : WORLD_POS;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD;
};

cbuffer CameraCB : register(b0)
{
	CameraData CamData;
}

cbuffer EntityCB : register(b1)
{
	uint EntityIndex;
}

cbuffer MaterialCB : register(b2)
{
	MaterialParams MatParams;
}

cbuffer LightSpaceCB : register(b3)
{
	float4x4 WorldToLightClip;
}

Texture2D AlbedoTexture : register(t0);
Texture2D MetallicRoughnessTexture : register(t1);
Texture2D NormalTexture : register(t2);
StructuredBuffer<Light> Lights : register(t3);
Texture2D Shadowmap : register(t4);
StructuredBuffer<EntityData> Entities : register(t5);


VertexOut VS(VertexInput IN)
{
	const float4 modelPos = float4(IN.Position, 1.0f);
	const float4 worldPos = mul(modelPos, Entities[EntityIndex].ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	VertexOut OUT;
	OUT.Position = clipPos;
	OUT.WorldPosition = worldPos.xyz;
	OUT.Normal = mul(IN.Normal, (float3x3) Entities[EntityIndex].ModelToWorld); // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	OUT.UV = IN.UV;
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
	float4 albedo = AlbedoTexture.Sample(s_AnisoWrap, IN.UV);

#ifdef ALPHA_DISCARD
	if (albedo.a < 0.05f)
		clip(-1.0f);
#endif // ALPHA_DISCARD

	//if (IsInShadow(IN.WorldPosition))
	//{
	//	return float4(0.0f, 0.0f, 0.0f, 1.0f);
	//}

	Material mat;
	mat.Albedo = albedo;
	mat.Albedo.rgb *= MatParams.AlbedoFactor;
	mat.FresnelR0 = MatParams.FresnelR0;
	mat.Roughness = MetallicRoughnessTexture.Sample(s_LinearWrap, IN.UV).g * MatParams.RoughnessFactor;
	mat.Roughness = min(0.99f, mat.Roughness);

	float3 normal = normalize(IN.Normal);
	float3 view = normalize(CamData.Position - IN.WorldPosition);

	float4 litColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	uint numLights, lightStride;
	Lights.GetDimensions(numLights, lightStride);

	for (uint i = 0; i < numLights; i++)
	{
		const Light l = Lights[i];
		switch (l.Type)
		{
		case 1:
			litColor.rgb += ComputeDirectionalLight(l, mat, normal, view);
			break;
		case 2:
			litColor.rgb += ComputePointLight(l, mat, IN.WorldPosition, normal, view);
			break;
		case 3:
			litColor.rgb += ComputeSpotLight(l, mat, IN.WorldPosition, normal, view);
			break;
		case 4:
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