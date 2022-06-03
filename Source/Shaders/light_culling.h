#include "shared_definitions.h"
#include "scene.h"

#define VISIBLE_LIGHT_END 0xffffffff

uint2 GetNumTiles(SceneInfo sceneInfo)
{
	const uint2 tileSizeVec = uint2(TILE_SIZE, TILE_SIZE);
	const uint2 numTiles = (uint2(sceneInfo.ScreenSize) + tileSizeVec - uint2(1, 1)) / tileSizeVec;
	return numTiles;
}

uint GetOffsetFromTileIndex(SceneInfo sceneInfo, uint2 tileIndex)
{
	const uint2 numTiles = GetNumTiles(sceneInfo);
	const uint tileStride = MAX_LIGHTS_PER_TILE + 1;
	return numTiles.x * tileStride * tileIndex.y + tileIndex.x * tileStride;
}

uint2 GetTileIndexFromPosition(float3 position)
{
	const float2 tileIndexF = position.xy / TILE_SIZE;
	return uint2(tileIndexF.x, tileIndexF.y);
}