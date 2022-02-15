#include "scene.h"

cbuffer CameraCB : register(b0)
{
	CameraData CamData;
}

cbuffer EntityCB : register(b1)
{
	EntityData EntData;
}

float4 VS(float3 Position : SV_POSITION) : SV_POSITION
{
	const float4 modelPos = float4(Position, 1.0f);
	const float4 worldPos = mul(modelPos, EntData.ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	return clipPos;
}