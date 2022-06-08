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

cbuffer Constants : register(b0)
{
	Camera CamData;
}

TextureCube SkyboxTexture : register(t0);

VertexOut VS(VertexInput IN)
{
	const float3 viewPos = mul(IN.Position, (float3x3) CamData.WorldToView);
	const float4 clipPos = mul(float4(viewPos, 1.0f), CamData.ViewToClip);
	VertexOut OUT;
	OUT.Position = clipPos;
	OUT.SkyRay = IN.Position;
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	return SkyboxTexture.Sample(s_LinearWrap, IN.SkyRay);
}