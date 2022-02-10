#pragma once

#include "Render/RenderAPI.h"

using BufferID = uint32_t;

struct Buffer
{
	// DX
	ComPtr<ID3D11Buffer> Handle;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11UnorderedAccessView> UAV;

	// Data
	uint64_t ByteSize;
	uint64_t ElementStride;
};

using TextureID = uint32_t;

struct Texture
{
	ComPtr<ID3D11Texture2D> Handle;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11UnorderedAccessView> UAV;
};

namespace GFX
{
	const Buffer& GetBuffer(BufferID id);
	const Texture& GetTexture(TextureID id);

	Buffer& CreateBuffer(BufferID& id);
	Texture& CreateTexture(TextureID& id);

	void ClearStorage();
}