#include "scene.h"

cbuffer CameraCB : register(b0)
{
	Camera CamData;
}

StructuredBuffer<Entity> Entities : register(t0);
StructuredBuffer<Drawable> Drawables : register(t1);

float4 VS(float3 Position : SV_POSITION, uint DrawableIndex : DRAWABLE_INDEX) : SV_POSITION
{
	const Drawable d = Drawables[DrawableIndex];

	const float4 modelPos = float4(Position, 1.0f);
	const float4 worldPos = mul(modelPos, Entities[d.EntityIndex].ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);
	const float4 clipPosWithJitter = clipPos + float4(CamData.Jitter, 0.0f, 0.0f) * clipPos.w;

	return clipPosWithJitter;
}