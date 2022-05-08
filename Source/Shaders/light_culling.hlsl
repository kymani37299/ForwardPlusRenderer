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

StructuredBuffer<Light> Lights : register(t0);
RWStructuredBuffer<uint> VisibleLights : register(u0);

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CS(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID)
{
	// TODO: Actually cull lights

	uint2 tileIndex = groupID.xy;
	uint writeOffset = GetOffsetFromTileIndex(TiledCullingInfoData, tileIndex);
	for (uint i = 0; i < 128; i++)
	{
		VisibleLights[writeOffset + i] = i;
	}
	VisibleLights[writeOffset + 128] = VISIBLE_LIGHT_END;
}