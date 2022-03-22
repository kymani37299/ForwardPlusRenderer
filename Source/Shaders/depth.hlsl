#include "scene.h"

cbuffer CameraCB : register(b0)
{
	CameraData CamData;
}

StructuredBuffer<EntityData> Entities : register(t0);

float4 VS(float3 Position : SV_POSITION, uint2 DrawableIndex : DRAWABLE_INDEX) : SV_POSITION
{
	const float4 modelPos = float4(Position, 1.0f);
	const float4 worldPos = mul(modelPos, Entities[DrawableIndex.x].ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	return clipPos;
}