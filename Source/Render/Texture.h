#pragma once

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"

struct ID3D11ShaderResourceView;

namespace GFX
{
	// Creation
	TextureID LoadTexture(const std::string& path, uint64_t creationFlags);
	TextureID CreateTexture(uint32_t width, uint32_t height, uint64_t creationFlags, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, const void* initData = nullptr);

	// Info
	ID3D11ShaderResourceView* DX_SRV(TextureID textureID);

	// Static samplers
	ID3D11SamplerState** GetStaticSamplers();
	uint16_t GetStaticSamplersNum();
}