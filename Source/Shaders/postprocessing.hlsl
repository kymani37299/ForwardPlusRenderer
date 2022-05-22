#include "samplers.h"

struct PostprocessingSettings
{
	float Exposure;
};

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

VertexOut VS(VertexInput IN)
{
	VertexOut OUT;
	OUT.pos = float4(IN.pos, 0.0f, 1.0f);
	OUT.uv = IN.uv;
	return OUT;
}

#ifdef TAA

Texture2D CurrentFrame : register(t0);
Texture2D LastFrame : register(t1);
Texture2D MotionVectors : register(t2);

float4 PS(VertexOut IN) : SV_Target
{
	const float2 motionVector = MotionVectors.Sample(s_PointWrap, IN.uv).xy;
	const float3 currentColor = CurrentFrame.Sample(s_PointWrap, IN.uv).xyz;
	const float3 lastColor = LastFrame.Sample(s_LinearWrap, IN.uv + motionVector).xyz;
	const float modulationFactor = 0.8;
	const float3 finalColor = lerp(currentColor, lastColor, modulationFactor);
	return float4(finalColor, 1.0f);
}

#endif // TAA

#ifdef TONEMAPPING

cbuffer PPSettingsCB : register(b0)
{
	PostprocessingSettings PP_Settings;
}

Texture2D HDRTexture : register(t0);

float4 PS(VertexOut IN) : SV_Target
{
	const float3 hdrColor = HDRTexture.Sample(s_LinearWrap, IN.uv).rgb;
	
#ifdef REINHARD
	const float3 color = hdrColor / (hdrColor + float3(1.0f,1.0f,1.0f));
#endif

#ifdef EXPOSURE
	// vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
	const float3 color = float3(1.0f, 1.0f, 1.0f) - exp(-hdrColor * PP_Settings.Exposure);
#endif 

	return float4(color, 1.0f);
}

#endif // TONEMAPPING