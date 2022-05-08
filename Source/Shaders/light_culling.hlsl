#include "scene.h"
#include "light_culling.h"

cbuffer SceneInfoCB : register(b0)
{
	SceneInfo SceneInfoData;
}

cbuffer TiledCullingInfoCB : register(b1)
{
	TiledCullingInfo TiledCullingInfoData;
}

cbuffer CameraCB : register(b2)
{
	Camera CamData;
}

StructuredBuffer<Light> Lights : register(t0);
Texture2D<float> DepthTexture : register(t1);

RWStructuredBuffer<uint> VisibleLights : register(u0);

groupshared uint gsMinDepth;
groupshared uint gsMaxDepth;
groupshared uint gsVisibleLightCount;
groupshared float4 gsFrustumPlanes[6];

groupshared uint gsVisibleLightIndexes[MAX_LIGHTS_PER_TILE];

float LinearizeDepth(float4x4 viewToClip, float depth)
{
	return (0.5f * viewToClip[3][2]) / (depth + 0.5f * viewToClip[2][2] - 0.5f);
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CS(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 localThreadID : SV_GroupThreadID)
{
	// Step 1: Initialization

	const uint threadCount = TILE_SIZE * TILE_SIZE;
	const uint2 pixelCoord = threadID.xy;
	const uint2 tileIndex = groupID.xy;
	const uint localIndex = localThreadID.y * TILE_SIZE + localThreadID.x;
	const bool isMainThread = localThreadID.x == 0 && localThreadID.y == 0;
	const float4x4 worldToClip = mul(CamData.WorldToView, CamData.ViewToClip);

	if (isMainThread)
	{
		gsMinDepth = 0xffffffff;
		gsMaxDepth = 0;
		gsVisibleLightCount = 0;
	}

#ifdef USE_BARRIERS
	AllMemoryBarrierWithGroupSync();
#endif

	// Step 2: Min and max depth

	float maxDepth, minDepth;
	const uint3 readPixelCoord = uint3(min(pixelCoord.x, TiledCullingInfoData.ScreenSize.x - 1), min(pixelCoord.y, TiledCullingInfoData.ScreenSize.y - 1), 0);
	const float depth = DepthTexture.Load(readPixelCoord);
	const float linearizedDepth = LinearizeDepth(CamData.ViewToClip, depth);
	const uint depthUint = asuint(linearizedDepth);

	InterlockedMin(gsMinDepth, depthUint);
	InterlockedMax(gsMaxDepth, depthUint);

#ifdef USE_BARRIERS
	AllMemoryBarrierWithGroupSync();
#endif

	// Step 3: Calculate frutum planes

	if (isMainThread)
	{
		minDepth = asfloat(gsMinDepth);
		maxDepth = asfloat(gsMaxDepth);
	
		const float2 negativeStep = (2.0f * float2(tileIndex)) / float2(TiledCullingInfoData.TileCount);
		const float2 positiveStep = (2.0f * float2(tileIndex + uint2(1, 1))) / float2(TiledCullingInfoData.TileCount);
	
		gsFrustumPlanes[0] = float4(1.0, 0.0, 0.0, 1.0 - negativeStep.x);		// Left
		gsFrustumPlanes[1] = float4(-1.0, 0.0, 0.0, -1.0 + positiveStep.x);		// Right
		gsFrustumPlanes[2] = float4(0.0, 1.0, 0.0, 1.0 - negativeStep.y);		// Bottom
		gsFrustumPlanes[3] = float4(0.0, -1.0, 0.0, -1.0 + positiveStep.y);		// Top
		gsFrustumPlanes[4] = float4(0.0, 0.0, -1.0, -minDepth);					// Near
		gsFrustumPlanes[5] = float4(0.0, 0.0, 1.0, maxDepth);					// Far
	
		// Trasnform
		for (uint i = 0; i < 6; i++) {
			const float4x4 transform = i < 4 ? worldToClip : CamData.WorldToView;
			gsFrustumPlanes[i] = mul(gsFrustumPlanes[i], transform);
			gsFrustumPlanes[i] /= length(gsFrustumPlanes[i].xyz);
		}
	}

#ifdef USE_BARRIERS
	AllMemoryBarrierWithGroupSync();
#endif

	// Step 4: Culling : Paralelizing against the lights

	const uint numPasses = (SceneInfoData.NumLights + threadCount - 1) / threadCount;
	for (uint i = 0; i < numPasses; i++)
	{
		const uint lightIndex = i * threadCount + localIndex;
		if (lightIndex >= SceneInfoData.NumLights)
			break;
	
		bool isVisible = false;
		const Light l = Lights[lightIndex];
	
		if (l.Type == LIGHT_TYPE_POINT)
		{
			const float4 position = float4(Lights[lightIndex].Position, 1.0f);
			const float radius = 1.0f; // TODO: Calculate radius
		
			float sd = 0.0; // Signed distance
			for (uint j = 0; j < 6; j++)
			{
				sd = dot(position, gsFrustumPlanes[j]) + radius;
				if (sd <= 0.0f) break;
			}
		
			isVisible = sd > 0.0f;
		}
		// TODO: else if(l.Type == LIGHT_TYPE_SPOT)
		//else
		{
			isVisible = true;
		}
	
		if (isVisible)
		{
			uint writeOffset;
			InterlockedAdd(gsVisibleLightCount, 1, writeOffset);
	
			if (writeOffset < MAX_LIGHTS_PER_TILE)
				gsVisibleLightIndexes[writeOffset] = lightIndex;
		}
	}

#ifdef USE_BARRIERS
	AllMemoryBarrierWithGroupSync();
#endif

	// Step 5: Write results

	if (isMainThread)
	{
		const uint writeOffset = GetOffsetFromTileIndex(TiledCullingInfoData, tileIndex);
		const uint visibleLightCount = min(gsVisibleLightCount, MAX_LIGHTS_PER_TILE);
		for (uint i = 0; i < visibleLightCount; i++)
		{
			VisibleLights[writeOffset + i] = gsVisibleLightIndexes[i];
		}
		VisibleLights[writeOffset + visibleLightCount] = VISIBLE_LIGHT_END;
	}
}