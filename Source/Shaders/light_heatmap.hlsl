#include "light_culling.h"

#define HIGH_LIGHT_COUNT 128

struct VertexInput
{
	float2 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

cbuffer TiledCullingInfoCB : register(b0)
{
	TiledCullingInfo TiledCullingInfoData;
}

StructuredBuffer<uint> VisibleLights : register(t0);

VertexOut VS(VertexInput IN)
{
	VertexOut OUT;
	OUT.pos = float4(IN.pos, 0.0f, 1.0f);
	OUT.uv = IN.uv;
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	const uint2 tileIndex = GetTileIndexFromPosition(IN.pos);
	const uint visibleLightOffset = GetOffsetFromTileIndex(TiledCullingInfoData, tileIndex);
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