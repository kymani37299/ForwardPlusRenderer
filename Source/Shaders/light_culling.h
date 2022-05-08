#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 1024
#define VISIBLE_LIGHT_END 0xffffffff

struct TiledCullingInfo
{
	uint2 TileCount;
};

uint GetOffsetFromTileIndex(TiledCullingInfo tiledCullingInfo, uint2 tileIndex)
{
	const uint tileStride = MAX_LIGHTS_PER_TILE + 1;
	return tiledCullingInfo.TileCount.x * tileStride * tileIndex.y + tileIndex.x * tileStride;
}

uint2 GetTileIndexFromPosition(float3 position)
{
	const float2 tileIndexF = position.xy / TILE_SIZE;
	return uint2(tileIndexF.x, tileIndexF.y);
}