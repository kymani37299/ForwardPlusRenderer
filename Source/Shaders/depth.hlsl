#include "scene.h"
#include "util.h"

struct VertexOut
{
	float4 Position : SV_POSITION;
	float4 CurrentFramePosition : CURR_POS;
	float4 LastFramePosition : LAST_FRAME_POS;
};

cbuffer CameraCB : register(b0)
{
	Camera CamData;
}

cbuffer CameraCBLastFrame : register(b1)
{
	Camera CamDataLastFrame;
}

cbuffer SceneInfoCB : register(b1)
{
	SceneInfo SceneInfoData;
}

StructuredBuffer<Entity> Entities : register(t0);
StructuredBuffer<Drawable> Drawables : register(t1);

VertexOut VS(float3 Position : SV_POSITION, uint DrawableIndex : DRAWABLE_INDEX)
{
	const Drawable d = Drawables[DrawableIndex];

	// TODO: lastFrameModelToWorld - to use in last frame position
	const float4x4 modelToWorld = Entities[d.EntityIndex].ModelToWorld;

	VertexOut OUT;
	OUT.Position = GetClipPosWithJitter(Position, modelToWorld, CamData);
	OUT.CurrentFramePosition = GetClipPos(Position, modelToWorld, CamData);
	OUT.LastFramePosition = GetClipPos(Position, modelToWorld, CamDataLastFrame);
	return OUT;
}

float2 CalculateMotionVector(float4 newPosition, float4 oldPosition, float2 screenSize)
{
	oldPosition /= oldPosition.w;
	oldPosition.xy = (oldPosition.xy + float2(1.0, 1.0)) / float2(2.0f, 2.0f);
	oldPosition.y = 1.0 - oldPosition.y;

	newPosition /= newPosition.w;
	newPosition.xy = (newPosition.xy + float2(1.0, 1.0)) / float2(2.0f, 2.0f);
	newPosition.y = 1.0 - newPosition.y;

	return (newPosition - oldPosition).xy;
}

float2 PS(VertexOut IN) : SV_TARGET
{
	return CalculateMotionVector(IN.CurrentFramePosition, IN.LastFramePosition, SceneInfoData.ScreenSize);
}