#include "scene.h"

cbuffer CameraCB : register(b0)
{
	CameraData CamData;
}

StructuredBuffer<EntityData> Entities : register(t0);
StructuredBuffer<Drawable> Drawables : register(t1);

float4 VS(float3 Position : SV_POSITION, uint DrawableIndex : DRAWABLE_INDEX) : SV_POSITION
{
	Drawable d = Drawables[DrawableIndex];

	const float4 modelPos = float4(Position, 1.0f);
	const float4 worldPos = mul(modelPos, Entities[d.EntityIndex].ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	return clipPos;
}