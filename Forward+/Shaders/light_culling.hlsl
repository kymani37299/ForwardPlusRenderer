#include "scene.h"
#include "culling.h"
#include "light_culling.h"
#include "util.h"

cbuffer Constants : register(b0)
{
	SceneInfo SceneInfoData;
	Camera CamData;
}

StructuredBuffer<Light> Lights : register(t0);
Texture2D<float> DepthTexture : register(t1);

RWStructuredBuffer<uint> VisibleLights : register(u0);

groupshared uint gsMinZ;
groupshared uint gsMaxZ;
groupshared uint gsVisibleLightCount;
groupshared ViewFrustum gsTileFrustum;

groupshared uint gsVisibleLightIndexes[MAX_LIGHTS_PER_TILE];

float ComputeLightRadius(Light light)
{
	switch (light.Type)
	{
	case LIGHT_TYPE_DIRECTIONAL:
		return 1000000.0f;

	case LIGHT_TYPE_POINT:
	case LIGHT_TYPE_SPOT: // TODO: This can be further optimized with direction
	{
		return light.Falloff.y;
	}
	case LIGHT_TYPE_AMBIENT:
		return 1000000.0f;
	}
	return 0.0f;
}

bool CullLight(Light light, ViewFrustum viewFrustum)
{
	bool isVisible = true;
	if (light.Type == LIGHT_TYPE_POINT)
	{
		BoundingSphere bs = CreateBoundingSphere(light.Position, ComputeLightRadius(light));
		isVisible = IsInViewFrustum(bs, viewFrustum);
	}
	return isVisible;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CS(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 localThreadID : SV_GroupThreadID)
{
	const float ZFar = 1000.0f;
	const float ZNear = 0.1f;

	// Step 1: Initialization

	const uint threadCount = TILE_SIZE * TILE_SIZE;
	const uint2 pixelCoord = threadID.xy;
	const uint2 tileIndex = groupID.xy;
	const uint localIndex = localThreadID.y * TILE_SIZE + localThreadID.x;
	const bool isMainThread = localThreadID.x == 0 && localThreadID.y == 0;

	if (isMainThread)
	{
		gsMinZ = 0xffffffff;
		gsMaxZ = 0;
		gsVisibleLightCount = 0;
	}

	AllMemoryBarrierWithGroupSync();

	// Step 2: Min and max Z

	const uint3 readPixelCoord = uint3(min(pixelCoord.x, SceneInfoData.ScreenSize.x - 1), min(pixelCoord.y, SceneInfoData.ScreenSize.y - 1), 0);
	const float depth = DepthTexture.Load(readPixelCoord);
	
	const uint ZUint = asuint(depth);
	InterlockedMin(gsMinZ, ZUint);
	InterlockedMax(gsMaxZ, ZUint);

	AllMemoryBarrierWithGroupSync();

	// Step 3: Calculate frutum planes

	if (isMainThread)
	{
		const float minZ = asfloat(gsMinZ);
		const float maxZ = asfloat(gsMaxZ);

		const float2 tileSizeNormalized = float2(TILE_SIZE, TILE_SIZE) / float2(SceneInfoData.ScreenSize);

		// [0,1]
		// 0 1
		// 2 3
		float2 points_clip[4];
		points_clip[0] = tileIndex * tileSizeNormalized;
		points_clip[1] = (tileIndex + float2(1.0, 0.0)) * tileSizeNormalized;
		points_clip[2] = (tileIndex + float2(1.0, 1.0)) * tileSizeNormalized;
		points_clip[3] = (tileIndex + float2(1.0, 1.0)) * tileSizeNormalized;

		// [-1, 1]
		for (uint i = 0; i < 4; i++)
		{
			points_clip[i].y = 1.0f - points_clip[i].y;
			points_clip[i] = points_clip[i] * float2(2.0f, 2.0f) - float2(1.0f, 1.0f);
		}

		// Points must be in order: ftl ftr fbl fbr ntl ntr nbl nbr
		float3 points_world[8];
		for (uint i = 0; i < 4; i++)
		{
			float4 clipPos = mul(float4(points_clip[i], minZ, 1.0), CamData.ClipToWorld);
			points_world[i] = clipPos.xyz / clipPos.w;
		}
		for (uint i = 0; i < 4; i++)
		{
			float4 clipPos = mul(float4(points_clip[i], maxZ, 1.0), CamData.ClipToWorld);
			points_world[4 + i] = clipPos.xyz / clipPos.w;
		}

		gsTileFrustum = CreateViewFrustum(points_world);
	}

	AllMemoryBarrierWithGroupSync();

	// Step 4: Culling : Paralelizing against the lights

	// Ceil(SceneInfoData.NumLights / threadCount)
	const uint numLightsPerThread = (SceneInfoData.NumLights + threadCount - 1) / threadCount;
	const uint threadLightOffset = localIndex * numLightsPerThread;
	for (uint i = 0; i < numLightsPerThread; i++)
	{
		const uint lightIndex = i + threadLightOffset;
		if (lightIndex >= SceneInfoData.NumLights) break;
		const Light l = Lights[lightIndex];

		bool isVisible = CullLight(l, gsTileFrustum);
		if (isVisible)
		{
			uint writeOffset;
			InterlockedAdd(gsVisibleLightCount, 1, writeOffset);

			if (writeOffset < MAX_LIGHTS_PER_TILE)
				gsVisibleLightIndexes[writeOffset] = lightIndex;
		}
	}

	AllMemoryBarrierWithGroupSync();

	// Step 5: Write results

	if (isMainThread)
	{
		const uint writeOffset = GetOffsetFromTileIndex(SceneInfoData, tileIndex);
		const uint visibleLightCount = min(gsVisibleLightCount, MAX_LIGHTS_PER_TILE);
		for (uint i = 0; i < visibleLightCount; i++)
		{
			VisibleLights[writeOffset + i] = gsVisibleLightIndexes[i];
		}
		VisibleLights[writeOffset + visibleLightCount] = VISIBLE_LIGHT_END;
	}
}