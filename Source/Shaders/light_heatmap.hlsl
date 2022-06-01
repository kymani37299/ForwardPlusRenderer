#include "light_culling.h"
#include "scene.h"
#include "full_screen.h"

VS_IMPL;

#define HIGH_LIGHT_COUNT 30

cbuffer SceneInfoCB : register(b0)
{
	SceneInfo SceneInfoData;
}

StructuredBuffer<uint> VisibleLights : register(t0);

float4 PS(FCVertex IN) : SV_Target
{
	const uint2 tileIndex = GetTileIndexFromPosition(IN.pos);
	const uint visibleLightOffset = GetOffsetFromTileIndex(SceneInfoData, tileIndex);
	uint lightCount = 0;
	for (uint i = visibleLightOffset; VisibleLights[i] != VISIBLE_LIGHT_END; i++)
	{
		lightCount++;
	}
	const float lightValue = (float) lightCount / HIGH_LIGHT_COUNT;
	const float4 highColor = float4(1.0, 0.0, 0.0, 0.5);
	const float4 lowColor = float4(0.0, 0.0, 1.0, 0.5);
	return lerp(lowColor, highColor, lightValue);
}