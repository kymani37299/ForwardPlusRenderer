#include "samplers.h"
#include "full_screen.h"
#include "settings.h"

VS_IMPL;

#ifdef TAA

Texture2D CurrentFrame : register(t0);
Texture2D LastFrame : register(t1);
Texture2D MotionVectors : register(t2);

float4 PS(FCVertex IN) : SV_Target
{
	const float2 motionVector = MotionVectors.Sample(s_PointWrap, IN.uv).xy;
	const float3 currentColor = CurrentFrame.Sample(s_PointWrap, IN.uv).xyz;
	const float3 lastColor = LastFrame.Sample(s_LinearWrap, IN.uv + motionVector).xyz;
	const float modulationFactor = 0.5f;
	const float3 finalColor = lerp(currentColor, lastColor, modulationFactor);
	return float4(finalColor, 1.0f);
}

#endif // TAA

#ifdef TONEMAPPING

cbuffer Constants : register(b0)
{
	float Exposure;
}

Texture2D HDRTexture : register(t0);
Texture2D BloomTexture : register(t1);

float4 PS(FCVertex IN) : SV_Target
{
	float3 hdrColor = HDRTexture.Sample(s_LinearWrap, IN.uv).rgb;
	
#ifdef APPLY_BLOOM
	hdrColor += BloomTexture.Sample(s_LinearWrap, IN.uv).rgb;
#endif // APPLY_BLOOM

	const float3 color = float3(1.0f, 1.0f, 1.0f) - exp(-hdrColor * Exposure);

	return float4(color, 1.0f);
}

#endif // TONEMAPPING