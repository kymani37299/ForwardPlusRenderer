#pragma once

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"

struct ID3D11ShaderResourceView;

namespace GFX
{
	// Creation
	TextureID LoadTexture(const std::string& path);
	TextureID CreateTexture(uint32_t width, uint32_t height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, const void* initData = nullptr);

	RenderTargetID CreateRenderTarget(uint32_t width, uint32_t height, bool useDepthStencil = false, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	// Info
	ID3D11ShaderResourceView* DX_GetTextureSRV(TextureID textureID);
}