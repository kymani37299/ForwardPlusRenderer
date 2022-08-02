#pragma once

#include "scene.h"

struct DebugGeometry
{
	float4x4 ModelToWorld;
	float4 Color;
};

cbuffer Constants : register(b0)
{
	Camera CamData;
}

StructuredBuffer<DebugGeometry> InstanceData : register(t0);

struct VertexOut
{
	float4 Position : SV_POSITION;
	float4 Color : GEO_COLOR;
};

VertexOut VS(float3 position : MODEL_POSITION, uint instanceID : SV_InstanceID)
{
	const float4 modelPos = float4(position, 1.0f);
	const float4 worldPos = mul(modelPos, InstanceData[instanceID].ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);

	VertexOut OUT;
	OUT.Position = clipPos;
	OUT.Color = InstanceData[instanceID].Color;

	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	return IN.Color;
}