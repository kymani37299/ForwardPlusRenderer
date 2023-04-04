#pragma once

#include "Render/RenderAPI.h"
#include "Render/Resource.h"

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

struct TextureSubresourceView : public Texture
{
	Resource* Parent;

	uint32_t FirstMip;
	uint32_t LastMip;

	uint32_t FirstElement;
	uint32_t LastElement;
};

namespace GFX
{
	Texture* CreateTexture(uint32_t width, uint32_t height, RCF creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, ResourceInitData* initData = nullptr);
	Texture* CreateTextureArray(uint32_t width, uint32_t height, uint32_t numElements, RCF creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, std::vector<ResourceInitData*> initData = {});

	inline Texture* CreateTexture3D(uint32_t width, uint32_t height, uint32_t depth, RCF creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM)
 	{
		return CreateTextureArray(width, height, depth, creationFlags | RCF::Texture3D, numMips, format);
	}

	// After finishing make sure to bring back ResourceState as it was at the beggining
	TextureSubresourceView* GetTextureSubresource(Texture* resource, uint32_t firstMip, uint32_t lastMip, uint32_t firstElement, uint32_t lastElement);
	
	uint32_t GetSubresourceIndex(Texture* texture, uint32_t mipIndex, uint32_t sliceOrArrayIndex);
}