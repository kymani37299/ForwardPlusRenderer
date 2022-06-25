#include "full_screen.h"

VS_IMPL;

Texture2DMS<float> DepthTexture : register(t0);

cbuffer Constants : register(b0)
{
	uint2 ScreenSize;
}

float PS(FCVertex IN) : SV_Depth
{
	const uint2 readPixelCoord = IN.uv * ScreenSize;
	return DepthTexture.Load(readPixelCoord, 0);
}
