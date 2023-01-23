#include "shared_definitions.h"

#ifdef CLEAR_DEPTH

cbuffer PushConstants : register(b128)
{
	uint DepthWidth;
	uint DepthHeight;
}

RWTexture2D<float> DepthTexture : register(u0);

[numthreads(OPT_TILE_SIZE, OPT_TILE_SIZE, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	const uint2 texCoords = threadID.xy;
	if (texCoords.x >= DepthWidth || texCoords.y >= DepthHeight)
		return;

	DepthTexture[texCoords] = 1.0f;
}

#endif // CLEAR_DEPTH

#ifdef REPROJECT_DEPTH

#include "scene.h"
#include "util.h"

cbuffer Constants : register(b0)
{
	uint2 DepthSize;
	uint2 Padding;
	Camera LastFrameCamera;
	Camera CurrentFrameCamera;
}

Texture2D<float> LastFrameDepth : register(t0);
RWTexture2D<float> ReprojectedDepth : register(u0);

[numthreads(OPT_TILE_SIZE, OPT_TILE_SIZE, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	const uint2 texCoords = threadID.xy;

	if (texCoords.x >= DepthSize.x || texCoords.y >= DepthSize.y)
		return;

	const float2 depthUV = (float2(texCoords)+0.5f) / DepthSize;
	const float lfDepth = LastFrameDepth[texCoords];

	const float3 lfWorldPos = GetWorldPositionFromDepth(lfDepth, depthUV, LastFrameCamera);

	const float4 cfClipPos = GetClipPos(lfWorldPos, CurrentFrameCamera);
	const float2 cfUV = GetUVFromClipPosition(cfClipPos);

	const bool outOfScreen = cfUV.x < 0.0f || cfUV.x > 1.0f || cfUV.y < 0.0f || cfUV.y > 1.0f;
	if (!outOfScreen)
	{
		const float cfDepth = GetDepthFromClipPosition(cfClipPos);
		const uint2 cfTexCoords = cfUV * DepthSize;
		ReprojectedDepth[texCoords] = clamp(cfDepth, 0.0f, 1.0f);
	}
}

#endif // REPROJECT_DEPTH

#ifdef GENERATE_HZB

cbuffer PushConstants : register(b128)
{
	uint ReadMip;
	uint WriteMip;
	uint HzbWidth;
	uint HzbHeight;
}

Texture2D<float> HZB_IN : register(t0);
RWTexture2D<float> HZB_OUT : register(u0);

SamplerState s_HZBSampler : register(s0);

[numthreads(OPT_TILE_SIZE, OPT_TILE_SIZE, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	const uint2 texCoords = threadID.xy;
	const uint2 writeDims = uint2(HzbWidth >> WriteMip, HzbHeight >> WriteMip);
	if (texCoords.x >= writeDims.x || texCoords.y >= writeDims.y)
		return;

	const float offset = 0.5f / writeDims.x;
	const float2 readUV = texCoords / float2(writeDims);
	float maxValue = 0.0f;
	
	// Doing this four times since our Depth is not power of two so it can happen to miss some values when reducing size
	maxValue = max(maxValue, HZB_IN.SampleLevel(s_HZBSampler, readUV + float2(-offset, -offset), ReadMip));
	maxValue = max(maxValue, HZB_IN.SampleLevel(s_HZBSampler, readUV + float2(offset, -offset), ReadMip));
	maxValue = max(maxValue, HZB_IN.SampleLevel(s_HZBSampler, readUV + float2(-offset, offset), ReadMip));
	maxValue = max(maxValue, HZB_IN.SampleLevel(s_HZBSampler, readUV + float2(offset, offset), ReadMip));

	HZB_OUT[texCoords] = maxValue;
}

#endif // GENERATE_HZB