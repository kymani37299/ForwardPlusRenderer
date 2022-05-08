#include "light_culling.h"

cbuffer TiledCullingInfoCB : register(b0)
{
	TiledCullingInfo TiledCullingInfoData;
}

StructuredBuffer<uint> VisibleLights : register(t0);
RWStructuredBuffer<uint> TotalLights : register(u0);

[numthreads(1, 1, 1)]
void CS()
{
	uint visibleLights = 0;
	const uint2 numTiles = TiledCullingInfoData.TileCount;

	for (uint tileX = 0; tileX < numTiles.x; tileX++)
	{
		for (uint tileY = 0; tileY < numTiles.y; tileY++)
		{
			uint readOffset = GetOffsetFromTileIndex(TiledCullingInfoData, uint2(tileX, tileY));
			for (uint i = readOffset; VisibleLights[i] != VISIBLE_LIGHT_END; i++)
			{
				visibleLights++;
			}
		}
	}
	TotalLights[0] = visibleLights / (numTiles.x * numTiles.y);
}