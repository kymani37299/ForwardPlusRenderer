#include "samplers.h"
#include "scene.h"
#include "util.h"
#include "full_screen.h"

VS_IMPL;

cbuffer CameraCB : register(b0)
{
	Camera MainCamera;
}

cbuffer ShadowCameraCB : register(b1)
{
	Camera ShadowCamera;
}

cbuffer SceneInfoCB : register(b2)
{
	SceneInfo SceneInfoData;
}

#ifdef MULTISAMPLE_DEPTH
Texture2DMS<float> DepthTexture : register(t0);
#else
Texture2D<float> DepthTexture : register(t0);
#endif

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
		shadowFactor = isInShadow ? 0.2f : 1.0f;
	}
	return shadowFactor;
}

float4 PS(FCVertex IN) : SV_Target
{
	const uint3 pixelCoord = uint3(IN.uv * SceneInfoData.ScreenSize, 0);
#ifdef MULTISAMPLE_DEPTH
	const float depth = DepthTexture.Load(pixelCoord, 0);
#else
	const float depth = DepthTexture.Load(pixelCoord);
#endif
	const float3 worldPosition = GetWorldPositionFromDepth(depth, IN.uv, MainCamera);
	return CalculateShadowFactor(worldPosition);
}