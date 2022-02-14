#include "samplers.h"
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

struct CameraData
{
	float4x4 WorldToView;
	float4x4 ViewToClip;
	float3 Position;
};

struct EntityData
{
	float4x4 ModelToWorld;
};

cbuffer CameraCB : register(b0)
{
	CameraData CamData;
}

cbuffer EntityCB : register(b1)
{
	EntityData EntData;
}

Texture2D AlbedoTexture : register(t0);

VertexOut VS(VertexInput IN)
{
	const float4 modelPos = float4(IN.Position, 1.0f);
	const float4 worldPos = mul(modelPos, EntData.ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	VertexOut OUT;
	OUT.Position = clipPos;
	OUT.WorldPosition = worldPos.xyz;
	OUT.Normal = mul(IN.Normal, (float3x3) EntData.ModelToWorld); // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	OUT.UV = IN.UV;
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	// TO BE ADDED TO DATA
	const float4 ambientLight = 0.1f * float4(1.0f, 1.0f, 1.0f, 1.0f);

	Light l;
	l.Position = float3(0.0f, 10.0f, 0.0f);
	l.Direction = normalize(float3(-1.0f, -1.0f, -1.0f));
	l.Strength = float3(0.5f, 0.5f, 0.5f);
	l.Falloff = float2(0.0f, 0.0f);
	l.SpotPower = 0.0f;

	Material mat;
	mat.Albedo = AlbedoTexture.Sample(s_LinearWrap, IN.UV);
	mat.FresnelR0 = float3(0.05f, 0.05f, 0.05f);
	mat.Roughness = 0.2f;
	//

	float3 normal = normalize(IN.Normal);
	float3 view = normalize(CamData.Position - IN.WorldPosition);

	float4 litColor = ambientLight * mat.Albedo;
	litColor.rgb += ComputeDirectionalLight(l, mat, normal, view);

#ifdef USE_ALPHA
	litColor.a = mat.Albedo.a;
#else
	litColor.a = 1.0f;
#endif // USE_ALPHA

	return litColor;
}