#include "samplers.h"
#include "scene.h"

struct VertexInput
{
	float3 Position : SV_POSITION;
};

struct VertexOut
{
	float4 Position : SV_POSITION;
	float3 SkyRay : SKY_RAY;
};

cbuffer CameraCB : register(b0)
{
	Camera CamData;
}

TextureCube SkyboxTexture : register(t0);

VertexOut VS(VertexInput IN)
{
	// TODO: Do not do transpose
	float4x4 worldToViewNoTranslation = transpose(CamData.WorldToView);
	worldToViewNoTranslation[0].w = 0.0;
	worldToViewNoTranslation[1].w = 0.0;
	worldToViewNoTranslation[2].w = 0.0;
	worldToViewNoTranslation = transpose(worldToViewNoTranslation);

	const float4 worldPos = float4(IN.Position, 1.0f);
	const float4 viewPos = mul(worldPos, worldToViewNoTranslation);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	VertexOut OUT;
	OUT.Position = clipPos;
	OUT.SkyRay = worldPos;
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	return SkyboxTexture.Sample(s_LinearWrap, IN.SkyRay);
}