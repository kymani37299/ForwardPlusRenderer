#pragma once

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"

struct ID3D11ShaderResourceView;

namespace GFX
{
	static constexpr uint32_t MAX_MIPS = 0;

	// Loading
	TextureID LoadTextureHDR(const std::string& path, uint64_t creationFlags);
	TextureID LoadTexture(ID3D11DeviceContext* context, const std::string& path, uint64_t creationFlags, uint32_t numMips = 1);
	TextureID LoadCubemap(const std::string& path, uint64_t creationFlags);
	
	// Creation
	TextureID CreateTexture(uint32_t width, uint32_t height, uint64_t creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, const void* initData = nullptr);
	TextureID CreateTextureArray(uint32_t width, uint32_t height, uint32_t arraySize, uint64_t creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, std::vector<const void*> initData = {});

	// Info
	uint32_t GetNumMips(TextureID textureID);
}