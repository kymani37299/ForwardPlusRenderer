#include "scene.h"
#include "util.h"

cbuffer CameraCB : register(b0)
{
	Camera CamData;
}

StructuredBuffer<Entity> Entities : register(t0);
StructuredBuffer<Drawable> Drawables : register(t1);

float4 VS(float3 Position : SV_POSITION, uint DrawableIndex : DRAWABLE_INDEX) : SV_POSITION
{
	const Drawable d = Drawables[DrawableIndex];
	return GetClipPosWithJitter(Position, Entities[d.EntityIndex].ModelToWorld, CamData);
}