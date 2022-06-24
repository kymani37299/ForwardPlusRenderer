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

cbuffer Constants : register(b0)
{
	float4x4 WorldToClip;
}

TextureCube Skybox : register(t0);

VS_Output VS(VS_Input IN)
{
	VS_Output OUT;
	OUT.pos = mul(float4(IN.pos, 1.0), WorldToClip);
	OUT.localPos = IN.pos;
	return OUT;
}

float4 PS(VS_Output IN) : SV_Target
{
	float3 irradiance = 0.0f;

	const float3 normal = normalize(IN.localPos);
	
	float3 up = float3(0.0f, 1.0f, 0.0f);
	const float3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));

	const float sampleDelta = 0.025f;
	float nSamples = 0.0f;

	for (float phi = 0.0f; phi < 2.0f * 3.1415f; phi += sampleDelta)
	{
		for (float theta = 0.0f; theta < 0.5f * 3.1415f; theta += sampleDelta)
		{
			const float sinTheta = sin(theta);
			const float cosTheta = cos(theta);

			const float3 tangentSample = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
			const float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

			irradiance += Skybox.Sample(s_LinearWrap, sampleVec).rgb * sinTheta * cosTheta;
			nSamples++;
		}
	}

	irradiance = (3.1415f * irradiance) / nSamples;

	return float4(irradiance, 1.0f);
}
