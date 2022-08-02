#ifndef UTIL_H
#define UTIL_H

#include "scene.h"

float4 GetClipPos(float3 worldPos, Camera camera)
{
	const float4 viewPos = mul(float4(worldPos, 1.0f), camera.WorldToView);
	const float4 clipPos = mul(viewPos, camera.ViewToClip);
	return clipPos;
}

float4 GetClipPosWithJitter(float3 worldPos, Camera camera)
{
	const float4 clipPos = GetClipPos(worldPos, camera);
	const float4 clipPosWithJitter = clipPos + float4(camera.Jitter, 0.0f, 0.0f) * clipPos.w;
	return clipPosWithJitter;
}

float3 GetWorldPositionFromDepth(float depth, float2 screenUV, Camera camera)
{
	// [0,1]
	float3 normalizedPosition = float3(screenUV, depth);

	// Flip the y because screenUV is 0.0 in upper left and we want down left
	normalizedPosition.y = 1.0f - normalizedPosition.y;

	// [-1,1] for xy and [0,1] for z
	const float4 clipSpacePosition = float4(normalizedPosition * float3(2.0f, 2.0f, 1.0f) + float3(-1.0f, -1.0f, 0.0f), 1.0f);
	const float4 worldPosition = mul(clipSpacePosition, camera.ClipToWorld);

	return worldPosition.xyz / worldPosition.w;
}

float2 GetUVFromClipPosition(float4 clipPosition)
{
	clipPosition.xyz /= clipPosition.w;

	// [-1,1] -> [0,1]
	float2 uv = clipPosition.xy * 0.5f + 0.5f;
	uv.y = 1.0f - uv.y;
	return uv;
}

#endif // UTIL_H