#include "samplers.h"
#include "scene.h"
#include "util.h"
#include "full_screen.h"

VS_IMPL;

cbuffer Constants : register(b0)
{
	Camera MainCamera;
	Camera ShadowCamera;
	SceneInfo SceneInfoData;
}

Texture2D<float> DepthTexture : register(t0);
Texture2D<float> Shadowmap : register(t1);

float CalculateShadowFactor(float3 worldPosition)
{
	float4 shadowmapPosition = GetClipPos(worldPosition, ShadowCamera);
	shadowmapPosition.xyz /= shadowmapPosition.w;

	const float2 shadowmapUV = GetUVFromClipPosition(shadowmapPosition.xyz);

	const bool inShadowMap = shadowmapUV.x < 1.0f && shadowmapUV.x > 0.0f && shadowmapUV.y < 1.0f && shadowmapUV.y > 0.0f;

	float shadowFactor = 1.0f;
	if (inShadowMap)
	{
		const float depthBias = 0.05f;
		const float shadowmapDepth = Shadowmap.Sample(s_LinearWrap, shadowmapUV) + depthBias;
		const bool isInShadow = shadowmapPosition.z > shadowmapDepth;
		shadowFactor = isInShadow ? 0.1f : 1.0f;
	}
	return shadowFactor;
}

float4 PS(FCVertex IN) : SV_Target
{
	// TODO: Sample this instead of load
	const uint3 pixelCoord = uint3(IN.uv * SceneInfoData.ScreenSize, 0);
	const float depth = DepthTexture.Load(pixelCoord);

	const float3 worldPosition = GetWorldPositionFromDepth(depth, IN.uv, MainCamera);
	return CalculateShadowFactor(worldPosition);
}