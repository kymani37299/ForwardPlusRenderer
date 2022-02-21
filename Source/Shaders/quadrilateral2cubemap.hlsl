#include "samplers.h"

struct VS_Input
{
	float3 pos : POS;
};

struct VS_Output
{
	float4 pos : SV_POSITION;
	float3 localPos : LOCAL_POS;
};

cbuffer CameraCB : register(b0)
{
	float4x4 WorldToClip;
}

Texture2D quadrilateralTexture : register(t0);

float2 SprericalToUV(float3 coords)
{
	const float2 invAtan = float2(0.1591, 0.3183);
	float2 uv = float2(atan2(coords.z, coords.x), asin(coords.y));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

VS_Output VS(VS_Input IN)
{
	VS_Output OUT;
	OUT.pos = mul(float4(IN.pos, 1.0), WorldToClip);
	OUT.localPos = IN.pos;
	return OUT;
}

float4 PS(VS_Output IN) : SV_Target
{
	float2 uv = SprericalToUV(normalize(IN.localPos));
	return quadrilateralTexture.Sample(s_LinearWrap, uv);
	//return float4(uv, 1.0f, 1.0f);
}