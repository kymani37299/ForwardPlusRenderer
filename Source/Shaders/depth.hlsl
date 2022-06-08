#include "scene.h"
#include "util.h"
#include "pipeline.h"

#ifdef ALPHA_DISCARD
#include "samplers.h"
#endif

struct VertexOut
{
	float4 Position : SV_POSITION;

#ifdef MOTION_VECTORS
	float4 CurrentFramePosition : CURR_POS;
	float4 LastFramePosition : LAST_FRAME_POS;
#endif // MOTION_VECTORS

#ifdef ALPHA_DISCARD
	float2 Texcoord : TEXCOORD;
	nointerpolation uint MaterialIndex : MAT_INDEX;
#endif // ALPHA_DISCARD
};

cbuffer Constants : register(b0)
{
	Camera CamData;
#ifndef SHADOWMAP
	Camera CamDataLastFrame;
	SceneInfo SceneInfoData;
#endif // SHADOWMAP
}

#ifdef MOTION_VECTORS

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
#endif // MOTION_VECTORS

VertexOut VS(VertexPipelineInput IN)
{
    const Drawable d = Drawables[IN.DrawableInstance];
    const Vertex vert = GetWorldSpaceVertex(IN);
	
	VertexOut OUT;
    OUT.Position = GetClipPosWithJitter(vert.Position, CamData);

#ifdef MOTION_VECTORS
    OUT.CurrentFramePosition = GetClipPos(vert.Position, CamData);
    OUT.LastFramePosition = GetClipPos(vert.Position, CamDataLastFrame);
#endif // MOTION_VECTORS

#ifdef ALPHA_DISCARD
	OUT.Texcoord = vert.Texcoord;
	OUT.MaterialIndex = d.MaterialIndex;
#endif
	return OUT;
}



#ifdef MOTION_VECTORS
float2 PS(VertexOut IN) : SV_TARGET
#else
void PS(VertexOut IN)
#endif // MOTION_VECTORS
{
#ifdef ALPHA_DISCARD
	const Material matParams = Materials[IN.MaterialIndex];
	const float alpha = Textures.Sample(s_AnisoWrap, float3(IN.Texcoord, matParams.Albedo)).a;

	if (alpha < 0.05f)
		clip(-1.0f);
#endif

#ifdef MOTION_VECTORS
	return CalculateMotionVector(IN.CurrentFramePosition, IN.LastFramePosition, SceneInfoData.ScreenSize);
#endif // MOTION_VECTORS
}