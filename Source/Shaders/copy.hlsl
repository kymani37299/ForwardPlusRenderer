#include "samplers.h"
#include "full_screen.h"

Texture2D src : register(t0);

VS_IMPL;

float4 PS(FCVertex IN) : SV_Target
{
	return src.Sample(s_LinearWrap, IN.uv);
}