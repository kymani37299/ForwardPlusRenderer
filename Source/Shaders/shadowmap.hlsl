#include "scene.h"

cbuffer ShadowCameraCB : register(b0)
{
	float4x4 WorldToClip;
}

cbuffer EntityCB : register(b1)
{
	EntityData EntData;
}

float4 VS(float3 Position : SV_POSITION) : SV_POSITION
{
	const float4 modelPos = float4(Position, 1.0f);
	const float4 worldPos = mul(modelPos, EntData.ModelToWorld);
	const float4 clipPos = mul(worldPos, WorldToClip);
	return clipPos;
}