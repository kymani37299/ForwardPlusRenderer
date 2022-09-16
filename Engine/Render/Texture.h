#pragma once

#include "Render/RenderAPI.h"
#include "Render/Resource.h"

struct GraphicsContext;

struct Texture : public Resource
{
	DXGI_FORMAT Format;
	uint32_t Width;
	uint32_t Height;
	uint32_t DepthOrArraySize;
	uint32_t NumMips;

	uint32_t RowPitch;
	uint32_t SlicePitch;
};

struct TextureSubresource : public Texture
{
	uint32_t FirstMip;
	uint32_t MipCount;

	uint32_t FirstElement;
	uint32_t ElementCount;
};

namespace GFX
{
	Texture* CreateTexture(uint32_t width, uint32_t height, uint64_t creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, ResourceInitData* initData = nullptr);
	Texture* CreateTextureArray(uint32_t width, uint32_t height, uint32_t numElements, uint64_t creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, std::vector<ResourceInitData*> initData = {});

	inline Texture* CreateTexture3D(uint32_t width, uint32_t height, uint32_t depth, uint64_t creationFlags, uint32_t numMips, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM)
 	{
		return CreateTextureArray(width, height, depth, creationFlags | RCF_Texture3D, numMips, format);
	}

	Texture* LoadTextureHDR(const std::string& path, uint64_t creationFlags);
	Texture* LoadTexture(GraphicsContext& context, const std::string& path, uint64_t creationFlags, uint32_t numMips = 1);
	Texture* LoadCubemap(const std::string& path, uint64_t creationFlags);

	TextureSubresource* CreateTextureSubresource(Texture* resource, uint32_t mipBegin, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount);

	uint32_t GetSubresourceIndex(Texture* texture, uint32_t mipIndex, uint32_t sliceOrArrayIndex);
}