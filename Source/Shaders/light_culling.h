#define TILE_SIZE 16
#define VISIBLE_LIGHT_END 0xffffffff

struct TiledCullingInfo
{
	uint MaxLightsPerTile;
	uint2 TileCount;
};

uint GetOffsetFromTileIndex(TiledCullingInfo tiledCullingInfo, uint2 tileIndex)
{
	const uint tileStride = tiledCullingInfo.MaxLightsPerTile + 1;
	return tiledCullingInfo.TileCount.x * tileStride * tileIndex.y + tileIndex.x * tileStride;
}

uint2 GetTileIndexFromPosition(float3 position)
{
	float2 tileIndexF = position.xy / TILE_SIZE;
	return uint2(tileIndexF.x, tileIndexF.y);
}