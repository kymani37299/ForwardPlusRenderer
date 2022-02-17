#pragma once

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"

struct ID3D11ShaderResourceView;

namespace GFX
{
	static constexpr uint32_t MAX_MIPS = 0;

	// Creation
	TextureID LoadTextureHDR(const std::string& path, uint64_t creationFlags);
	TextureID LoadTexture(ID3D11DeviceContext* context, const std::string& path, uint64_t creationFlags, uint32_t numMips = 1);
	TextureID CreateTexture(uint32_t width, uint32_t height, uint64_t creationFlags, uint32_t numMips = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, const void* initData = nullptr);

	// Info
	uint32_t GetNumMips(TextureID textureID);
	ID3D11Texture2D* DX_Texture2D(TextureID textureID);
	ID3D11ShaderResourceView* DX_SRV(TextureID textureID);

	// Static samplers
	ID3D11SamplerState** GetStaticSamplers();
	uint16_t GetStaticSamplersNum();
}