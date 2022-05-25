#include "scene.h"
#include "util.h"

#ifdef ALPHA_DISCARD
#include "samplers.h"
#endif

struct VertexIn
{
	float3 Position : SV_POSITION;
	uint DrawableIndex : DRAWABLE_INDEX;
#ifdef ALPHA_DISCARD
	float2 Texcoord : TEXCOORD;
#endif // ALPHA_DISCARD
};

struct VertexOut
{
	float4 Position : SV_POSITION;
	float4 CurrentFramePosition : CURR_POS;
	float4 LastFramePosition : LAST_FRAME_POS;

#ifdef ALPHA_DISCARD
	float2 Texcoord : TEXCOORD;
	nointerpolation uint MaterialIndex : MAT_INDEX;
#endif // ALPHA_DISCARD
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

#ifdef ALPHA_DISCARD
Texture2DArray Textures : register(t3);
StructuredBuffer<Material> Materials : register(t4);
#endif

VertexOut VS(VertexIn IN)
{
	const Drawable d = Drawables[IN.DrawableIndex];

	// TODO: lastFrameModelToWorld - to use in last frame position
	const float4x4 modelToWorld = Entities[d.EntityIndex].ModelToWorld;

	VertexOut OUT;
	OUT.Position = GetClipPosWithJitter(IN.Position, modelToWorld, CamData);
	OUT.CurrentFramePosition = GetClipPos(IN.Position, modelToWorld, CamData);
	OUT.LastFramePosition = GetClipPos(IN.Position, modelToWorld, CamDataLastFrame);

#ifdef ALPHA_DISCARD
	OUT.Texcoord = IN.Texcoord;
	OUT.MaterialIndex = d.MaterialIndex;
#endif
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
#ifdef ALPHA_DISCARD
	const Material matParams = Materials[IN.MaterialIndex];
	const float alpha = Textures.Sample(s_AnisoWrap, float3(IN.Texcoord, matParams.Albedo)).a;

	if (alpha < 0.05f)
		clip(-1.0f);
#endif

	return CalculateMotionVector(IN.CurrentFramePosition, IN.LastFramePosition, SceneInfoData.ScreenSize);
}