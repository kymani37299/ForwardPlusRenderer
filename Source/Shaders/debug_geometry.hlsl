#pragma once

#include "scene.h"

struct DebugGeometry
{
	float4x4 ModelToWorld;
	float4 Color;
};

cbuffer DebugGeometryCB : register(b0)
{
	DebugGeometry DebugGeoData;
}

cbuffer CameraCB : register(b1)
{
	Camera CamData;
}

float4 VS(float3 position : SV_POSITION) : SV_Position
{
	const float4 modelPos = float4(position, 1.0f);
	const float4 worldPos = mul(modelPos, DebugGeoData.ModelToWorld);
	const float4 viewPos = mul(worldPos, CamData.WorldToView);
	const float4 clipPos = mul(viewPos, CamData.ViewToClip);
	return clipPos;
}

float4 PS(float4 position : SV_POSITION) : SV_Target
{
	return DebugGeoData.Color;
}