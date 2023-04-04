#ifdef TEXTURE_PREVIEW

#include "full_screen.h"

VS_IMPL;

cbuffer Constants : register(b0)
{
	float RangeMin;
	float RangeMax;
	uint SelectedMip;
};

SamplerState s_PointWrap : register(s0);
Texture2D InputTexture : register(t0);

float4 PS(FCVertex IN) : SV_Target
{
	float4 inputColor = InputTexture.SampleLevel(s_PointWrap, IN.uv, SelectedMip);
	inputColor -= RangeMin;
	inputColor = inputColor / (RangeMax - RangeMin);
	inputColor = clamp(inputColor, 0.0f, 1.0f);

#ifdef SHOW_ALPHA
	return float4(inputColor.a, 0.0f, 0.0f, 1.0f);
#else
	return float4(inputColor.rgb, 1.0f);
#endif
}

#endif // TEXTURE_PREVIEW

#ifdef READ_RANGE

#include "shared_definitions.h"

cbuffer PushConstants : register(b128)
{
	uint TexWidth;
	uint TexHeight;
}


Texture2D InputTexture : register(t0);
RWStructuredBuffer<uint> RangeBuffer : register(u0);

[numthreads(OPT_TILE_SIZE, OPT_TILE_SIZE, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	const uint2 texCoords = threadID.xy;
	if (texCoords.x >= TexWidth || texCoords.y >= TexHeight)
		return;

	// TODO: Check all colors
	const float red = asuint(InputTexture[texCoords].r);

	InterlockedMin(RangeBuffer[0], red);
	InterlockedMax(RangeBuffer[1], red);
}

#endif // READ_RANGE